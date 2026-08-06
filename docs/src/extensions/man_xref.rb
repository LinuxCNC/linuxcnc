# docs/src/extensions/man_xref.rb
#
# Asciidoctor treeprocessor that turns manpage cross-references in the
# conventional name(section) form (e.g. "halcmd(1)") into link: macros
# pointing at the sibling HTML page man<section>/<name>.<section>.html.
# Running on the parsed AST (before conversion) rewrites text in AsciiDoc
# source space, so code blocks, monospace spans, passthroughs and existing
# links are excluded by node context instead of by guessing at tag
# boundaries in finished HTML.
#
#   * Index-gated: a token is linked only when a page <name>.<section> exists
#     in the troff tree, so false positives never resolve and stay plain text
#     ("feed(2)"/"arc(3)" enum values in motion(9); external open(2)/udev(8);
#     typo'd or renamed API names).
#   * Never links a page to itself.
#   * Skips verbatim blocks; inside inline text skips monospace spans,
#     pass:[] passthroughs, link:/xref:/image: macros, URLs and <<xrefs>>.
#   * Case-insensitive match ("AXIS(1)" => axis.1); visible text kept verbatim.
#
# Used for both the HTML manpages (sibling links under man/) and the narrative
# manuals (User/HAL/Integrator), which reference man pages in the same
# name(section) form and now link across into ../man/man<N>/.
#
# manxref-root (passed from the Submakefile) points at the troff man tree
# docs/build/man[/<lang>], whose man<N>/ dirs enumerate every page including
# generated component pages and .so stubs.  Absent => no-op, safe to always load.
# manxref-linkbase is the relative path from the page to those man<N>/ dirs
# (default "../" for a sibling manpage; narrative pages pass their own depth).

require 'asciidoctor'
require 'asciidoctor/extensions'

module LinuxCNCDocs
  class ManXref < Asciidoctor::Extensions::Treeprocessor
    SECTIONS = %w[1 3 9].freeze

    # One  name(section)  token.  Name starts with a letter/underscore so
    # version-like "3.5(1)" never matches; the section is a single digit.
    TOKEN = /\b([A-Za-z_][A-Za-z0-9_.\-]*)\((\d)\)/.freeze

    # Inline source spans that must never be rewritten.  Note compat-mode
    # legacy 'quotes' are emphasis (<em>), not code, so they stay linkable.
    PROTECTED_SPAN = %r{(
        `[^`\n]*`
      | pass:\[[^\]\n]*\]
      | (?:link|xref|image):[^\s\[]*\[[^\]\n]*\]
      | <<[^>\n]*>>
      | https?://[^\s\[]+(?:\[[^\]\n]*\])?
    )}x.freeze

    @index_cache = {}
    class << self; attr_reader :index_cache; end

    # Build  "name-downcased\tsection" => "man<N>/<name>.<N>.html"  from
    # the troff man tree.  Filenames are the authoritative existence list.
    def self.build_index(root)
      key = File.expand_path(root)
      cached = index_cache[key]
      return cached if cached

      idx = {}
      SECTIONS.each do |sec|
        dir = File.join(root, "man#{sec}")
        next unless File.directory?(dir)
        suffix = ".#{sec}"
        Dir.foreach(dir) do |fn|
          next unless fn.end_with?(suffix)
          name = fn[0...-suffix.length]
          next if name.empty?
          idx["#{name.downcase}\t#{sec}"] = "man#{sec}/#{fn}.html"
        end
      end
      index_cache[key] = idx
    end

    def process(document)
      return unless document.backend.start_with?('html')

      root = document.attr('manxref-root')
      return if root.nil? || root.empty?

      idx = self.class.build_index(root)
      return if idx.empty?

      base = document.attr('manxref-linkbase')
      base = '../' if base.nil? || base.empty?

      ctx = {
        idx: idx,
        base: base,
        self_name: (document.attr('mantitle') || '').downcase,
        self_vol: (document.attr('manvolnum') || '').to_s,
      }
      document.blocks.each { |blk| rewrite_block(blk, ctx) }
      nil
    end

    private

    # ListItem, ListTerm and Table::Cell #text getters apply inline
    # substitutions; read and write the raw text to avoid double substitution.
    def raw_text(node)
      node.instance_variable_get(:@text)
    end

    def set_raw_text(node, text)
      node.instance_variable_set(:@text, text)
    end

    def rewrite_block(blk, ctx)
      case blk.context
      when :list_item
        set_raw_text(blk, rewrite_line(raw_text(blk), ctx)) if blk.text?
      when :table
        rewrite_table(blk, ctx)
      else
        blk.lines.map! { |line| rewrite_line(line, ctx) } if blk.content_model == :simple
      end
      return unless blk.blocks?
      blk.blocks.each do |child|
        if child.is_a?(Array)
          # dlist entry: [terms, description]
          terms, desc = child
          terms.each { |t| set_raw_text(t, rewrite_line(raw_text(t), ctx)) } if terms
          rewrite_block(desc, ctx) if desc
        else
          rewrite_block(child, ctx)
        end
      end
    end

    def rewrite_table(tbl, ctx)
      (tbl.rows.head + tbl.rows.body + tbl.rows.foot).each do |row|
        row.each do |cell|
          next if cell.style == :asciidoc
          set_raw_text(cell, rewrite_line(raw_text(cell), ctx))
        end
      end
    end

    # Link tokens only in unprotected text (protected spans land on odd
    # indices after the split).
    def rewrite_line(line, ctx)
      line.split(PROTECTED_SPAN).each_with_index.map do |part, i|
        i.odd? ? part : part.gsub(TOKEN) { link_token(Regexp.last_match, ctx) }
      end.join
    end

    def link_token(match, ctx)
      whole = match[0]
      return whole if match[1].downcase == ctx[:self_name] && match[2] == ctx[:self_vol]
      href = ctx[:idx]["#{match[1].downcase}\t#{match[2]}"]
      # compat-mode swallows link-macro attributes; put the styling role
      # on a wrapping span instead of on the anchor.
      href ? %([.man-xref]#link:#{ctx[:base]}#{href}[#{whole}]#) : whole
    end
  end
end

Asciidoctor::Extensions.register do
  treeprocessor LinuxCNCDocs::ManXref
end
