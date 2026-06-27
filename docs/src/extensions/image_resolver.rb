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
# For HTML the resolver also rewrites every image reference (and click-to-
# enlarge link=) to a shared pool so each picture is stored once instead of
# copied into every language tree:
#   generic image       -> <cssrel>image/<srcrel>
#   localized foo_<lang> -> <cssrel><lang>/image/<srcrel>
# <srcrel> is the path relative to docs/src; <cssrel> (lcnc-cssrel) is the
# path back to the html/ root.  .html-images-stamp materialises the pool as
# real files (no symlinks).  The page language comes from the lcnc-lang
# attribute, not the path, since the Asciidoctor 2.0 CLI has no sourcemap
# option.  The PDF backend is unchanged (absolute paths).

require 'asciidoctor'
require 'asciidoctor/extensions'

module LinuxCNCDocs
  class ImageResolver < Asciidoctor::Extensions::Treeprocessor
    IMAGE_EXTS = %w[.png .svg .jpg .jpeg].freeze
    # Inline image: macros.  Group 1 is the target, group 2 the bracket
    # body (attributes, possibly carrying a link="..."); skip the block
    # image:: form (handled by find_by(:image)) and anything that already
    # looks like a URL or absolute path.
    INLINE_IMAGE_RE = /(?<![:\w])image:(?!:)([^\[\s]+)\[([^\]]*)\]/
    # A link="..." (or link=...,) reference inside an inline macro body.
    INLINE_LINK_RE = /\blink=("?)([^"\],]+)\1/

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

    # The language tags (including en) longest-first, for stripping the
    # leading staging-language segment off a build/adoc-relative path.
    def lang_tags(document)
      attr = document.attr('doc-languages')
      langs = attr.nil? || attr.empty? ? [] : attr.split(/\s+/)
      (['en'] + langs).sort_by { |l| -l.length }
    end

    # Page language from the lcnc-lang attribute (passed by both the HTML and
    # PDF rules); fall back to the source path if the attribute is absent.
    def detect_lang(document, src, lang_re)
      l = document.attr('lcnc-lang')
      return l if l && !l.empty?
      lang_re && (m = lang_re.match(src.to_s)) ? m[1] : 'en'
    end

    def process(document)
      # PDF embedding needs absolute paths so prawn-svg / image reading
      # can find the file at convert time; HTML rewrites to the pool.
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
      return if target.start_with?('<') # literal example placeholder
      return if target.include?('{')    # leave macros alone

      src = node.file || node.document.attr('docfile')
      return unless src
      base_dir = File.dirname(File.expand_path(src))
      lang_re = lang_re_for(node.document)
      lang = detect_lang(node.document, src, lang_re)

      if node.document.backend == 'pdf'
        abs = resolve_target(target, base_dir, lang, lang_re)
        return unless abs
        node.set_attr('target', abs)
        apply_height_as_width(node, abs)
        apply_default_width(node)
        apply_default_alignment(node)
        return
      end

      # HTML: rewrite to the shared image pool.
      pooled = pool_target(target, base_dir, lang, lang_re, node.document)
      node.set_attr('target', pooled) if pooled
      rewrite_link_attr(node, base_dir, lang, lang_re)
    end

    # The click-to-enlarge link= on a block image points at the full-size
    # image; send it to the pool too.  resolve returning nil (a link to a
    # page/anchor, not an image file) leaves it untouched.
    def rewrite_link_attr(node, base_dir, lang, lang_re)
      link = node.attr('link')
      return unless link
      return if link.empty?
      return if link.start_with?('http://', 'https://', '/', '#', '<')
      return if link.include?('{')
      pooled = pool_target(link, base_dir, lang, lang_re, node.document)
      node.set_attr('link', pooled) if pooled
    end

    # Resolve a reference to the absolute source file (PDF path), reusing
    # the locale swap + English fallback.
    def resolve_target(target, base_dir, lang, lang_re)
      if lang != 'en'
        swapped = swap_lang(target, lang)
        if swapped != target
          abs = resolve_candidate(File.expand_path(swapped, base_dir), lang_re)
          return abs if abs
        end
      end
      resolve_candidate(File.expand_path(target, base_dir), lang_re)
    end

    # Compute the html pool path for a reference, or nil if it cannot be
    # resolved / lies outside the source trees (leave it untouched then).
    def pool_target(ref, base_dir, lang, lang_re, document)
      abs = resolve_pool_file(ref, base_dir, lang, document)
      return nil unless abs
      srcrel = pool_srcrel(abs, document)
      return nil unless srcrel
      cssrel = document.attr('lcnc-cssrel') || ''
      localized = lang != 'en' &&
                  File.basename(srcrel).match?(/_#{Regexp.escape(lang)}(\.[A-Za-z0-9]+)\z/)
      localized ? "#{cssrel}#{lang}/image/#{srcrel}" : "#{cssrel}image/#{srcrel}"
    end

    # Resolve a reference to a real file, probing the staged language tree,
    # docs/src, then the English build tree (generated SVGs live only there),
    # preferring a localized foo_<lang> variant.  Probing docs/src means an
    # inline image on a translated page resolves even though .adoc-images-stamp
    # stages only block images.
    def resolve_pool_file(ref, base_dir, lang, document)
      bases = pool_bases(base_dir, document)
      if lang != 'en'
        swapped = swap_lang(ref, lang)
        if swapped != ref
          bases.each do |b|
            r = probe_file(File.expand_path(swapped, b))
            return r if r
          end
        end
      end
      bases.each do |b|
        r = probe_file(File.expand_path(ref, b))
        return r if r
      end
      nil
    end

    def probe_file(path)
      return path if File.file?(path)
      resolve_extension(path)
    end

    # Candidate base directories for a page: the staged adoc dir itself,
    # plus the matching docs/src dir and the English build/adoc dir.
    def pool_bases(base_dir, document)
      list = [base_dir]
      srcdir = document.attr('lcnc-srcdir')
      adocdir = document.attr('lcnc-adocdir')
      if srcdir && adocdir
        bd = File.expand_path(base_dir)
        if bd.start_with?(adocdir + '/')
          rel = bd[(adocdir.length + 1)..-1] # e.g. "de/config" or "en/config"
          lang_tags(document).each do |l|
            if rel == l
              rel = ''
              break
            elsif rel.start_with?(l + '/')
              rel = rel[(l.length + 1)..-1]
              break
            end
          end
          list << (rel.empty? ? srcdir : File.join(srcdir, rel))
          list << File.join(adocdir, 'en', rel)
        end
      end
      list.uniq
    end

    # Path of the resolved image relative to docs/src.  resolve_candidate
    # returns a file either under docs/src or under the staging build/adoc
    # tree (build/adoc/en/... or build/adoc/<lang>/...); strip whichever
    # root applies, and the leading staging-language segment.
    def pool_srcrel(abs, document)
      srcdir = document.attr('lcnc-srcdir')
      adocdir = document.attr('lcnc-adocdir')
      p = File.expand_path(abs)
      return p[(srcdir.length + 1)..-1] if srcdir && p.start_with?(srcdir + '/')
      if adocdir && p.start_with?(adocdir + '/')
        rel = p[(adocdir.length + 1)..-1]
        lang_tags(document).each do |l|
          return rel[(l.length + 1)..-1] if rel.start_with?(l + '/')
        end
        return rel
      end
      nil
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
      src = block.file || block.document.attr('docfile')
      return unless src
      base_dir = File.dirname(File.expand_path(src))
      lang_re = lang_re_for(block.document)
      lang = detect_lang(block.document, src, lang_re)
      pdf = block.document.backend == 'pdf'
      document = block.document

      # :paragraph / :literal / :sidebar etc. carry source in .lines (Array<String>).
      if block.respond_to?(:lines=) && block.lines.is_a?(Array) && !block.lines.empty?
        changed = false
        new_lines = block.lines.map { |ln| rewrite_inline(ln, base_dir, lang, lang_re, pdf, document) { changed = true } }
        block.lines = new_lines if changed
      end

      # :list_item carries source in .text (String).
      if block.respond_to?(:text=) && block.instance_variable_defined?(:@text)
        old = block.instance_variable_get(:@text)
        if old.is_a?(String) && !old.empty?
          changed = false
          new_text = rewrite_inline(old, base_dir, lang, lang_re, pdf, document) { changed = true }
          block.text = new_text if changed
        end
      end
    end

    def rewrite_inline(text, base_dir, lang, lang_re, pdf, document)
      text.gsub(INLINE_IMAGE_RE) do
        full = Regexp.last_match(0)
        target = Regexp.last_match(1)
        body = Regexp.last_match(2)
        next full if target.start_with?('http://', 'https://', '/', '<')
        next full if target.include?('{')

        if pdf
          abs = resolve_target(target, base_dir, lang, lang_re)
          next full unless abs
          yield if block_given?
          next "image:#{abs}[#{body}]"
        end

        new_target = pool_target(target, base_dir, lang, lang_re, document)
        new_body = body.gsub(INLINE_LINK_RE) do
          q = Regexp.last_match(1)
          lval = Regexp.last_match(2)
          next Regexp.last_match(0) if lval.start_with?('http://', 'https://', '/', '#', '<') || lval.include?('{')
          lp = pool_target(lval, base_dir, lang, lang_re, document)
          lp ? "link=#{q}#{lp}#{q}" : Regexp.last_match(0)
        end
        if new_target || new_body != body
          yield if block_given?
          "image:#{new_target || target}[#{new_body}]"
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
      # A pinned height is an explicit size (already turned into pdfwidth by
      # apply_height_as_width where readable); don't override it with 75%.
      return if node.attr('height')
      node.set_attr('pdfwidth', '75%')
    end

    # asciidoctor-pdf sizes images only by the width family and ignores the
    # height attribute, so when only a height is given, convert it to the
    # equivalent pdfwidth via the file's intrinsic aspect ratio.
    def apply_height_as_width(node, abs)
      return if node.context == :inline_image
      return if node.attr('pdfwidth') || node.attr('scaledwidth') ||
                node.attr('width') || node.attr('scale')
      h = node.attr('height')
      return unless h && h.to_s.match?(/\A\d+(\.\d+)?\z/)
      dims = intrinsic_size(abs)
      return unless dims && dims[1] > 0
      px_w = h.to_f * dims[0] / dims[1]
      node.set_attr('pdfwidth', format('%gpx', px_w))
    end

    # Intrinsic [width, height] in px for the formats we ship, without an image
    # library.  nil when the size can't be determined.
    def intrinsic_size(path)
      return nil unless path && File.file?(path)
      case File.extname(path).downcase
      when '.png'  then png_size(path)
      when '.jpg', '.jpeg' then jpeg_size(path)
      when '.svg'  then svg_size(path)
      end
    rescue StandardError
      nil
    end

    def png_size(path)
      data = File.binread(path, 24)
      return nil unless data && data.byteslice(0, 8) == "\x89PNG\r\n\x1A\n".b
      w, h = data.byteslice(16, 8).unpack('N2')
      (w && h) ? [w, h] : nil
    end

    def jpeg_size(path)
      File.open(path, 'rb') do |f|
        return nil unless f.read(2) == "\xFF\xD8".b
        while (marker = f.read(2))
          break unless marker.getbyte(0) == 0xFF
          code = marker.getbyte(1)
          len = f.read(2)&.unpack1('n')
          break unless len
          # SOF0..SOF15 carry the frame size (skip the non-frame markers).
          if code >= 0xC0 && code <= 0xCF && ![0xC4, 0xC8, 0xCC].include?(code)
            seg = f.read(5)
            h, w = seg.byteslice(1, 4).unpack('n2')
            return [w, h]
          end
          f.seek(len - 2, IO::SEEK_CUR)
        end
      end
      nil
    end

    def svg_size(path)
      head = File.read(path, 2048)
      return nil unless head
      tag = head[/<svg\b[^>]*>/m]
      return nil unless tag
      w = tag[/\bwidth="([\d.]+)/, 1]
      h = tag[/\bheight="([\d.]+)/, 1]
      return [w.to_f, h.to_f] if w && h
      if (vb = tag[/\bviewBox="([^"]+)"/, 1])
        nums = vb.split(/[\s,]+/).map(&:to_f)
        return [nums[2], nums[3]] if nums.length == 4 && nums[3] > 0
      end
      nil
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

  # Asciidoctor emits image width/height as HTML attributes, but the bundled
  # stylesheet's `img{height:auto}` (an author rule) outranks them in the
  # cascade, so the requested height is dropped.  CSS alone can't recover it, so
  # mirror the width/height attributes into an inline style, which wins.  Bare
  # numbers become px; values with a unit or % pass through.
  class ImageDimensionStyler < Asciidoctor::Extensions::Postprocessor
    IMG_TAG_RE = /<(img|object)\b[^>]*>/i
    DIM_RE = ->(name) { /\b#{name}="([^"]*)"/i }

    def process(document, output)
      # Only the HTML backend hands the postprocessor a String of markup; the
      # PDF converter passes its document object, so leave non-String output be.
      return output unless output.is_a?(::String)
      return output unless document.backend == 'html5' || document.basebackend?('html')
      output.gsub(IMG_TAG_RE) { |tag| restyle(tag) }
    end

    def restyle(tag)
      decls = []
      %w[width height].each do |dim|
        m = DIM_RE.call(dim).match(tag)
        next unless m
        v = m[1].strip
        next if v.empty?
        decls << "#{dim}:#{css_length(v)}"
      end
      return tag if decls.empty?

      style = decls.join(';')
      if (sm = /\bstyle="([^"]*)"/i.match(tag))
        existing = sm[1].sub(/;\s*\z/, '')
        merged = existing.empty? ? style : "#{existing};#{style}"
        tag.sub(/\bstyle="[^"]*"/i, %(style="#{merged}"))
      else
        tag.sub(/<(img|object)\b/i, %(<\\1 style="#{style}"))
      end
    end

    # A bare integer/decimal is a pixel count (HTML attribute semantics);
    # anything that already names a unit or percentage is left untouched.
    def css_length(v)
      v.match?(/\A\d+(\.\d+)?\z/) ? "#{v}px" : v
    end
  end
end

Asciidoctor::Extensions.register do
  treeprocessor LinuxCNCDocs::ImageResolver
  postprocessor LinuxCNCDocs::ImageDimensionStyler
end
