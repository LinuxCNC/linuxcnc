# docs/src/extensions/xref_resolver.rb
#
# Asciidoctor preprocessor that resolves bare <<anchor,Title>> cross-doc
# references to a link: macro pointing at the target's output file.
#
# We emit link: rather than the inter-document xref form (<<file.adoc#id>>)
# because the docs build with -a compat-mode, which disables that syntax and
# renders it as a literal in-page anchor.  link: carries no auto reftext, so
# we capture the target's reftext (heading or [[id,reftext]]) for bare uses.
#
# Wire-up (in Submakefile):
#   asciidoctor -r $(DOC_SRCDIR)/extensions/xref_resolver.rb \
#               -a xref-root=$(DOC_SRCDIR) \
#               ...
# For translated trees pass xref-root=$(DOC_SRCDIR)/<lang> so anchors from
# de/ don't leak into en/ etc.

require 'asciidoctor'
require 'asciidoctor/extensions'
require 'pathname'
require 'digest'
require 'json'
require 'fileutils'

module LinuxCNCDocs
  class XrefResolver < Asciidoctor::Extensions::Preprocessor
    INDEX_CACHE = {}

    # Anchor definition forms recognised:
    #   [[id]]              block anchor
    #   [[id,reftext]]      block anchor with reference text
    #   [#id]               shorthand id
    #   [id="foo"]          block-attribute id (single or double quotes)
    #   :id: foo            document-level id attribute (rare)
    ANCHOR_DEF = /
      \[\[ ([A-Za-z_][\w:.-]*) (?:,[^\]]*)? \]\]                            |
      \[\# ([A-Za-z_][\w:.-]*) (?:[.%][^\]]*)? \]                           |
      \[ (?:[^,\]]*,\s*)* id\s*=\s*["']? ([A-Za-z_][\w:.-]*) ["']? [,\]]    |
      ^:id:\s* ([A-Za-z_][\w:.-]*)
    /x

    # <<anchor>> or <<anchor,title>>.  The title match is lazy so it stops at
    # the closing >>, which means a lone '>' in the title (e.g. <foo>) is fine.
    XREF_USE = /
      <<
      ([A-Za-z_][\w:.-]*)
      (,.*?)?
      >>
    /xm

    CACHE_DIR = ENV['LINUXCNC_DOCS_XREF_CACHE'] || '/tmp/lcnc-xref-cache'

    # Bump when the index structure changes so stale caches are ignored.
    INDEX_VERSION = 3

    # include::path[] directive (literal path only; skip ones with attributes).
    INCLUDE_DEF = /^include::([^\[{]+)\[/

    # Section title ("== Heading") / block title (".Heading"), used as reftext
    # for a preceding anchor.
    SECTION_TITLE = /^=+\s+(\S.*?)\s*$/
    BLOCK_TITLE   = /^\.(?!\.)(\S.*?)\s*$/

    # Reference text carried inline on an anchor: [[id,reftext]].
    def self.inline_reftext(line, anchor)
      m = /\[\[#{Regexp.escape(anchor)}\s*,\s*([^\]]+?)\s*\]\]/.match(line)
      m && m[1]
    end

    # First heading/block title after an anchor (skipping blanks and block
    # attributes), used as its reftext.  nil if body content comes first.
    def self.lookahead_reftext(lines, i)
      j = i + 1
      while j < lines.length
        l = lines[j]
        if l.strip.empty? || l.start_with?('[')
          j += 1
          next
        end
        if (m = SECTION_TITLE.match(l)) || (m = BLOCK_TITLE.match(l))
          return m[1]
        end
        return nil
      end
      nil
    end

    # Page an anchor renders into: walk the include graph (parents maps a file
    # to its includers) up to the topmost non-Master_ ancestor.  An include::d
    # partial has no .html of its own; Master_* files only build the PDFs.
    def self.html_master(file, parents, seen = nil)
      seen ||= {}
      return file if seen[file]
      seen[file] = true
      includers = (parents[file] || []).reject { |p| File.basename(p).start_with?('Master_') }
      return file if includers.empty?
      html_master(includers.first, parents, seen)
    end

    # A link: macro's text is terminated by the first unescaped ']', so a ']'
    # in plain text must be written '\]'.  Inside a `...` code span asciidoctor
    # treats ']' literally (and '\]' would leak the backslash), so leave those
    # alone: match whole code spans first and only escape ']' outside them.
    def self.escape_link_text(text)
      text.gsub(/(`[^`]*`)|\]/) { $1 || '\\]' }
    end

    def self.build_index(root, exclude_re = nil)
      cache_key = [root, exclude_re&.source].compact.join('|')
      key = File.expand_path(cache_key)
      return INDEX_CACHE[key] if INDEX_CACHE.key?(key)

      cache_file = File.join(CACHE_DIR, "#{Digest::SHA1.hexdigest(key)}.json")
      paths = Pathname.glob(File.join(root, '**', '*.adoc'))
      if exclude_re
        root_p = Pathname.new(File.expand_path(root))
        paths = paths.reject do |p|
          rel = p.expand_path.relative_path_from(root_p).to_s
          exclude_re.match?(rel)
        end
      end
      mtime_max = paths.map { |p| p.mtime.to_f }.max || 0.0
      if File.exist?(cache_file)
        cached = JSON.parse(File.read(cache_file)) rescue nil
        if cached && cached['version'] == INDEX_VERSION &&
           cached['mtime'].to_f >= mtime_max
          INDEX_CACHE[key] = cached['index']
          return cached['index']
        end
      end

      # idx: anchor => { 'file' => abspath, 'text' => reftext|nil }.
      # parents: included file => its includers.
      idx = {}
      parents = Hash.new { |h, k| h[k] = [] }
      paths.each do |path|
        abspath = path.expand_path.to_s
        dir = File.dirname(abspath)
        lines = path.readlines
        lines.each_with_index do |line, i|
          if (inc = INCLUDE_DEF.match(line))
            parents[File.expand_path(inc[1].strip, dir)] << abspath
            next
          end
          line.scan(ANCHOR_DEF) do |a, b, c, d|
            anchor = a || b || c || d
            next unless anchor
            if idx[anchor] && idx[anchor]['file'] != abspath
              warn "xref_resolver: duplicate anchor '#{anchor}' in " \
                   "#{path} (already in #{idx[anchor]['file']})"
              next
            end
            reftext = inline_reftext(line, anchor) || lookahead_reftext(lines, i)
            idx[anchor] = { 'file' => abspath, 'text' => reftext }
          end
        end
      end

      # Redirect partial anchors to the page they render into.
      idx.each_value { |e| e['file'] = html_master(e['file'], parents) }

      FileUtils.mkdir_p(CACHE_DIR)
      File.write(cache_file, JSON.dump('version' => INDEX_VERSION,
                                       'mtime' => mtime_max, 'index' => idx))
      INDEX_CACHE[key] = idx
    end

    def process(document, reader)
      docfile = document.attr 'docfile'
      return reader unless docfile

      root = document.attr('xref-root') ||
             document.attr('docdir')    ||
             File.dirname(docfile)
      exclude_pat = document.attr('xref-exclude')
      exclude_re = exclude_pat && !exclude_pat.empty? ? Regexp.new(exclude_pat) : nil
      idx = self.class.build_index(File.expand_path(root), exclude_re)
      src_dir = Pathname.new(File.dirname(File.expand_path(docfile)))
      self_path = File.expand_path(docfile)
      # Output extension (.html for the html build, .pdf for asciidoctor-pdf).
      suffix = document.attr('outfilesuffix') || '.html'

      # Join lines first so multi-line <<...\n...>> is matched as one.
      joined = reader.lines.join("\n")

      rewritten = joined.gsub(XREF_USE) do
        full = Regexp.last_match(0)
        anchor = Regexp.last_match(1)
        tail   = Regexp.last_match(2) || ''
        target = idx[anchor]
        if target && target['file'] != self_path
          relpath = Pathname.new(target['file']).relative_path_from(src_dir).to_s
          out = relpath.sub(/\.adoc\z/, suffix)
          # explicit <<id,Title>> label, else captured reftext, else anchor.
          text = tail.empty? ? (target['text'] || anchor) : tail.sub(/\A,\s*/, '')
          "link:#{out}##{anchor}[#{self.class.escape_link_text(text)}]"
        else
          full
        end
      end

      Asciidoctor::PreprocessorReader.new(document, rewritten.split("\n", -1), reader.cursor)
    end
  end
end

Asciidoctor::Extensions.register do
  preprocessor LinuxCNCDocs::XrefResolver
end
