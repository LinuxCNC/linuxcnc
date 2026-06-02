# docs/src/extensions/image_resolver.rb
#
# Asciidoctor treeprocessor that resolves image targets the way the
# asciidoc-py + docs/src/image-wildcard pair did, plus a LinuxCNC-
# specific language-suffix swap:
#   - relative image paths in an included file are resolved relative to
#     that included file's directory (not the top-level master)
#   - missing extension is filled in by trying png/svg/jpg/jpeg
#   - when the .adoc being processed lives under docs/src/<lang>/ and
#     the macro points at `foo_en.ext`, the resolver looks for
#     `foo_<lang>.ext` first and falls back to `foo_en.ext` if the
#     translated variant is not in the tree.  Implements the naming
#     convention BsAtHome proposed on #4053: img_en.ext is the default,
#     img_<lang>.ext (when it exists) is the translated override.
#
# Requires the document to be loaded with sourcemap: true (passed on the
# CLI as -a sourcemap=true) so blocks expose .file.

require 'asciidoctor'
require 'asciidoctor/extensions'

module LinuxCNCDocs
  class ImageResolver < Asciidoctor::Extensions::Treeprocessor
    IMAGE_EXTS = %w[.png .svg .jpg .jpeg].freeze
    # Inline image: macros, with target as captured group 1.  Skip the
    # block image:: form (handled by find_by(:image)) and anything that
    # already looks like a URL or absolute path.
    INLINE_IMAGE_RE = /(?<![:\w])image:(?!:)([^\[\s]+)\[/
    # Language subdirectories of docs/src/.  Stripped from the path when
    # falling back to the English source tree for images that exist only
    # in the canonical location.  The Submakefile passes the authoritative
    # list via -a doc-languages='de es fr nb ru uk zh_CN' (derived from
    # po4a.cfg's [po4a_langs] line, the single source of truth).  When
    # the attribute is missing or empty the resolver returns nil and the
    # caller skips the locale rewrite; preferable to silently embedding
    # a stale hardcoded list that would drift away from po4a.cfg.

    # Per-document regex cache keyed by the language list string.  The
    # treeprocessor itself is frozen by Asciidoctor after registration,
    # so we cannot store state on the instance directly; use a thread-
    # local map instead (safe because asciidoctor invocations are
    # single-threaded per process).
    def lang_re_for(document)
      attr = document.attr('doc-languages')
      return nil if attr.nil? || attr.empty?
      cache = (Thread.current[:lcnc_lang_re_cache] ||= {})
      cache[attr] ||= begin
        langs = attr.split(/\s+/)
        %r{/src/(#{langs.sort_by { |l| -l.length }.map { |l| Regexp.escape(l) }.join('|')})/}
      end
    end

    def process(document)
      # PDF embedding needs absolute paths so prawn-svg / image reading
      # can find the file at convert time; HTML only needs the relative
      # rewrite when the lang-suffix swap picks a translated variant.
      walk(document)
      document
    end

    # asciidoctor-style table cells (cols="...a") parse their content as
    # an inner Document, so find_by on the master doesn't reach blocks
    # nested inside them.  Walk into each cell's inner_document too.
    def walk(doc)
      doc.find_by(context: :image) { |n| rewrite n }
      # Inline image: macros are part of block text and never appear as
      # standalone nodes in find_by; scan block-level source storage for
      # them.  Asciidoctor parks block text in :lines (literal/paragraph)
      # or :text (list_item).
      doc.find_by do |b|
        next unless b.file
        rewrite_inline_in_block(b)
        false
      end
      doc.find_by(context: :table_cell) do |c|
        inner = c.inner_document if c.respond_to?(:inner_document)
        walk(inner) if inner
      end
    end

    def rewrite(node)
      target = node.attr 'target'
      return unless target
      return if target.empty?
      return if target.start_with?('http://', 'https://', '/')
      return if target.include?('{') # leave macros alone

      src = node.file || node.document.attr('docfile')
      return unless src
      base_dir = File.dirname(File.expand_path(src))
      lang_re = lang_re_for(node.document)
      lang = lang_re && (m = lang_re.match(src.to_s)) ? m[1] : 'en'
      pdf = node.document.backend == 'pdf'

      # Try a language-specific variant of the filename first.
      if lang != 'en'
        swapped = swap_lang(target, lang)
        if swapped != target
          abs = resolve_candidate(File.expand_path(swapped, base_dir), lang_re)
          if abs
            node.set_attr('target', pdf ? abs : swapped)
            apply_default_width(node) if pdf
            apply_default_alignment(node) if pdf
            return
          end
        end
      end

      # No translated variant available (or English doc); the original
      # target stays for HTML, only PDF needs the absolute rewrite.
      return unless pdf
      abs = resolve_candidate(File.expand_path(target, base_dir), lang_re)
      return unless abs
      node.set_attr('target', abs)
      apply_default_width(node)
      apply_default_alignment(node) if pdf
    end

    # Rewrite an `*_en.<ext>` filename to `*_<lang>.<ext>`.  The check
    # is anchored to the basename so paths that happen to contain `_en`
    # earlier are not touched.
    def swap_lang(target, lang)
      target.sub(/_en(\.[A-Za-z0-9]+)\z/, "_#{lang}\\1")
    end

    # Try the requested path; fall back to the canonical English source
    # if the request points into a translated tree.  Image directories
    # under docs/src/<lang>/ are not always populated for every macro a
    # translated file references, but the English original at
    # docs/src/.../ usually exists.
    def resolve_candidate(path, lang_re)
      probe = ->(p) {
        return p if File.file?(p)
        r = resolve_extension(p)
        return r if r && File.file?(r)
        nil
      }
      r = probe.call(path)
      return r if r
      return nil unless lang_re
      fallback = path.sub(lang_re, '/src/')
      fallback != path ? probe.call(fallback) : nil
    end

    def rewrite_inline_in_block(block)
      base_dir = File.dirname(File.expand_path(block.file))
      lang_re = lang_re_for(block.document)
      lang = lang_re && (m = lang_re.match(block.file.to_s)) ? m[1] : 'en'
      pdf = block.document.backend == 'pdf'

      # :paragraph / :literal / :sidebar etc. carry source in .lines (Array<String>).
      if block.respond_to?(:lines=) && block.lines.is_a?(Array) && !block.lines.empty?
        changed = false
        new_lines = block.lines.map { |ln| rewrite_inline(ln, base_dir, lang, lang_re, pdf) { changed = true } }
        block.lines = new_lines if changed
      end

      # :list_item carries source in .text (String).
      if block.respond_to?(:text=) && block.instance_variable_defined?(:@text)
        old = block.instance_variable_get(:@text)
        if old.is_a?(String) && !old.empty?
          changed = false
          new_text = rewrite_inline(old, base_dir, lang, lang_re, pdf) { changed = true }
          block.text = new_text if changed
        end
      end
    end

    def rewrite_inline(text, base_dir, lang, lang_re, pdf)
      text.gsub(INLINE_IMAGE_RE) do
        full = Regexp.last_match(0)
        target = Regexp.last_match(1)
        next full if target.start_with?('http://', 'https://', '/')
        next full if target.include?('{')

        if lang != 'en'
          swapped = swap_lang(target, lang)
          if swapped != target
            abs = resolve_candidate(File.expand_path(swapped, base_dir), lang_re)
            if abs
              yield if block_given?
              next "image:#{pdf ? abs : swapped}["
            end
          end
        end

        next full unless pdf
        candidate = resolve_candidate(File.expand_path(target, base_dir), lang_re)
        if candidate
          yield if block_given?
          "image:#{candidate}["
        else
          full
        end
      end
    end

    # asciidoctor-pdf renders raster images at native pixel dimensions
    # interpreted as 72 DPI, then caps at content width.  Most of our
    # source PNGs are screenshots/diagrams sized for ~150 DPI display, so
    # the default behaviour blows them up to full text column width and
    # leaves big half-blank pages where they break across a page boundary.
    # dblatex defaulted to a smaller fit.  Approximate that by setting a
    # default pdfwidth when the source did not pin width/scaledwidth/pdfwidth.
    def apply_default_width(node)
      return if node.context == :inline_image
      return if node.attr('pdfwidth')
      return if node.attr('scaledwidth')
      return if node.attr('width')
      node.set_attr('pdfwidth', '75%')
    end
  
    # center images by default if no alignmen is given
    def apply_default_alignment(node)
      return if node.context == :inline_image
      return if node.attr('align')
      node.set_attr('align', 'center')
    end

    def resolve_extension(path)
      return path if File.file?(path)
      return nil unless File.extname(path).empty?
      IMAGE_EXTS.each do |e|
        c = path + e
        return c if File.file?(c)
      end
      nil
    end
  end
end

Asciidoctor::Extensions.register do
  treeprocessor LinuxCNCDocs::ImageResolver
end
