# docs/src/extensions/man_xref.rb
#
# Asciidoctor postprocessor that turns manpage cross-references in the
# conventional name(section) form (e.g. "halcmd(1)") into clickable links to
# the sibling HTML page ../man<section>/<name>.<section>.html.
#
#   * Index-gated: a token is linked only when a page <name>.<section> exists
#     in the troff tree, so false positives never resolve and stay plain text
#     ("feed(2)"/"arc(3)" enum values in motion(9); external open(2)/udev(8);
#     typo'd or renamed API names).
#   * Never links a page to itself.
#   * Skips text inside <a> <code> <pre> <script> <style> <head> <title> <h1>,
#     so code samples, tag attributes and existing links are left untouched.
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
  class ManXref < Asciidoctor::Extensions::Postprocessor
    # Manpage sections LinuxCNC ships and cross-references between.
    SECTIONS = %w[1 3 9].freeze

    # Elements whose text content must never be rewritten.
    PROTECTED = %w[a code pre script style head title h1].freeze

    # One  name(section)  token.  Name starts with a letter/underscore so
    # version-like "3.5(1)" never matches; the section is a single digit.
    TOKEN = /\b([A-Za-z_][A-Za-z0-9_.\-]*)\((\d)\)/.freeze

    # Cache the per-root index across the many pages of one asciidoctor run.
    @index_cache = {}
    class << self; attr_reader :index_cache; end

    # Build  "name-downcased\tsection" => "man<N>/<name>.html"  from the
    # troff man tree.  Filenames are the authoritative existence list.
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
          # Rendered HTML keeps the section in the filename: the troff page
          # man<N>/<name>.<N> becomes man<N>/<name>.<N>.html.
          idx["#{name.downcase}\t#{sec}"] = "man#{sec}/#{fn}.html"
        end
      end
      index_cache[key] = idx
    end

    # Split HTML into an alternating stream of text runs and tags, tracking a
    # stack of PROTECTED elements; yield only text runs that are safe to edit.
    def self.each_editable_text(html)
      depth = 0
      pos = 0
      out = +''
      html.scan(/([^<]+)|(<[^>]*>)/) do
        text, tag = Regexp.last_match(1), Regexp.last_match(2)
        if text
          out << (depth.zero? ? yield(text) : text)
        else
          out << tag
          m = /\A<\s*(\/?)\s*([A-Za-z][A-Za-z0-9]*)/.match(tag)
          if m && PROTECTED.include?(m[2].downcase) && !tag.end_with?('/>')
            depth += (m[1] == '/' ? -1 : 1)
            depth = 0 if depth < 0
          end
        end
        pos += 1
      end
      out
    end

    def process(document, output)
      root = document.attr('manxref-root')
      return output if root.nil? || root.empty?

      idx = self.class.build_index(root)
      return output if idx.empty?

      self_name = (document.attr('mantitle') || '').downcase
      self_vol  = (document.attr('manvolnum') || '').to_s

      # Relative path from this page to the man<N>/ dirs.  Manpages sit beside
      # each other under man/, so the default reaches a sibling section dir;
      # narrative pages pass their own depth-adjusted base (../man/, ../../man/).
      base = document.attr('manxref-linkbase')
      base = '../' if base.nil? || base.empty?

      self.class.each_editable_text(output) do |text|
        text.gsub(TOKEN) do
          whole = Regexp.last_match(0)
          name  = Regexp.last_match(1)
          sec   = Regexp.last_match(2)
          # Never link a page to itself.
          next whole if name.downcase == self_name && sec == self_vol
          href = idx["#{name.downcase}\t#{sec}"]
          href ? %(<a class="man-xref" href="#{base}#{href}">#{whole}</a>) : whole
        end
      end
    end
  end
end

Asciidoctor::Extensions.register do
  postprocessor LinuxCNCDocs::ManXref
end
