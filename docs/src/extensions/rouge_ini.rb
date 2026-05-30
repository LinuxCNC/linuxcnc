# docs/src/extensions/rouge_ini.rb
#
# Fixes Rouge's default INI lexer for LinuxCNC INI files:
#   - accepts `#`/`;` comments without a trailing newline (end of file/block),
#   - accepts leading whitespace before `key = value` lines,
#   - highlights the `#INCLUDE` directive as a preprocessor token.
#
# Reopening `state :basic do` would replace the upstream rules and lose the
# whitespace handler, so we rebuild it explicitly here.
#
# Loaded via the asciidoctor -r flag from the docs Submakefile.

require "rouge"

module Rouge
  module Lexers
    class INI < RegexLexer
      title "INI"
      desc "INI with LinuxCNC #INCLUDE + tolerant comments / whitespace"

      identifier = /[a-zA-Z_][a-zA-Z0-9_]*/

      state :includefile do
        # An include directive must start at the first character on the line
        # and has a strict line format with mandatory leading and optional
        # trailing whitespace arround the filename.
        rule %r/^(#INCLUDE)(\s+)(.*)(\s*)$/ do
          groups Keyword, Text, Str, Text
        end
      end

      state :root do
        mixin :includefile
        mixin :basic

        rule %r/(#{identifier})(\s*)(=)/ do
          groups Name::Property, Text, Punctuation
          push :value
        end

        rule %r/^[ \t]*[;#][^\n]*(?=\n|\z)/, Comment
        rule %r/(\[)(#{identifier})(\])(\s*)([;#].*)?/ do
          groups Punctuation, Name::Namespace, Punctuation, Text, Comment
        end
      end
    end
  end
end
