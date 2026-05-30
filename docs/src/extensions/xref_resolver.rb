# docs/src/extensions/xref_resolver.rb
#
# Asciidoctor preprocessor that resolves bare <<anchor,Title>> cross-doc
# references to qualified <<relpath/file.adoc#anchor,Title>> form, the way
# asciidoc-py used to do via objects/xref_<lang>.links.
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

    XREF_USE = /
      <<
      ([A-Za-z_][\w:.-]*)                           # bare anchor (no path)
      (,[^>]*?)?                                    # optional ,title
      >>
    /xm

    CACHE_DIR = ENV['LINUXCNC_DOCS_XREF_CACHE'] || '/tmp/lcnc-xref-cache'

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
        if cached && cached['mtime'].to_f >= mtime_max
          INDEX_CACHE[key] = cached['index']
          return cached['index']
        end
      end

      idx = {}
      paths.each do |path|
        path.each_line do |line|
          line.scan(ANCHOR_DEF) do |a, b, c, d|
            anchor = a || b || c || d
            next unless anchor
            target = path.expand_path.to_s
            if idx[anchor] && idx[anchor] != target
              warn "xref_resolver: duplicate anchor '#{anchor}' in " \
                   "#{path} (already in #{idx[anchor]})"
              next
            end
            idx[anchor] = target
          end
        end
      end

      FileUtils.mkdir_p(CACHE_DIR)
      File.write(cache_file, JSON.dump('mtime' => mtime_max, 'index' => idx))
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

      # Join lines first so multi-line <<...\n...>> is matched as one.
      joined = reader.lines.join("\n")

      rewritten = joined.gsub(XREF_USE) do
        full = Regexp.last_match(0)
        anchor = Regexp.last_match(1)
        tail   = Regexp.last_match(2) || ''
        # Pass through if line already looks qualified.
        next full if full.include?('.adoc#')
        target = idx[anchor]
        if target && target != self_path
          relpath = Pathname.new(target).relative_path_from(src_dir).to_s
          "<<#{relpath}##{anchor}#{tail}>>"
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
