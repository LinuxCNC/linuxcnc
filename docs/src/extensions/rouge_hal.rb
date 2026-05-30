# docs/src/extensions/rouge_hal.rb
#
# Rouge lexer for LinuxCNC HAL (Hardware Abstraction Layer) script files.
# Ported from docs/src/source-highlight/hal.lang (Michael Haberler, 2011).
#
# Highlights:
#   - halcmd commands (loadrt, net, setp, addf, ...)
#   - pin / signal names of the form  component.pin-name
#   - INI substitutions of the form   [SECTION]NAME (any case)
#   - environment variables           $VAR  $(VAR)
#   - assignment operators            =  =>  <=
#   - numbers in decimal, hex (0x..), octal (0o..), and binary (0b..)
#   - comments  (# ...)
#
# Loaded via the asciidoctor -r flag from the docs Submakefile.

require 'rouge'

module Rouge
  module Lexers
    class HAL < RegexLexer
      title 'HAL'
      desc 'LinuxCNC Hardware Abstraction Layer (halcmd)'
      tag 'hal'
      filenames '*.hal', '*.tcl.hal', '*.postgui.hal'
      mimetypes 'text/x-hal'

      COMMANDS = %w[
        loadrt unloadrt loadusr waitusr unloadusr
        newsig delsig sets gets stype ptype getp setp
        linkps linksp linkpp net unlinkp
        addf delf start stop
        show item save source
        alias unalias list
        lock unlock status help
        echo
        initf
      ].join('|').freeze

      identifier = /[a-zA-Z_][a-zA-Z0-9_]*/

      state :root do
        rule %r/\s+/, Text
        rule %r/#.*$/, Comment::Single

        # Commands as the first non-space token of a line
        rule %r/(?<=^|\s)(?:#{COMMANDS})\b/i, Keyword

        # Assignment operators
        rule %r/=>|<=|=/, Operator

        # INI substitution: [SECTION]NAME -- LinuxCNC accepts any case
        # for both the section name and the variable name (it normalises
        # internally), so the lexer follows the source-highlight ini.lang
        # convention of taking any letter/digit/underscore.
        rule %r/(\[)(#{identifier})(\])(#{identifier})/ do
          groups Punctuation, Name::Namespace, Punctuation, Name::Property
        end

        # Environment variables (POSIX shell rules: any case allowed)
        rule %r/\$\([A-Za-z_]\w*\)/, Name::Variable
        rule %r/\$[A-Za-z_]\w*/, Name::Variable

        # Pin/signal names: token with at least one dot
        rule %r/[A-Za-z_][\w-]*(?:\.[\w-]+)+/, Name::Attribute

        # Numbers
        # - hex / octal / binary integers (halcmd accepts 0x.., 0o.., 0b..)
        rule %r/[+-]?0[xX][0-9a-fA-F]+/, Num::Hex
        rule %r/[+-]?0[oO][0-7]+/, Num::Oct
        rule %r/[+-]?0[bB][01]+/, Num::Bin
        # - floats: must have either a decimal point or an exponent
        rule %r/[+-]?\d+\.\d+(?:[eE][+-]?\d+)?/, Num::Float
        rule %r/[+-]?\d+[eE][+-]?\d+/, Num::Float
        # - decimal integers (no exponent)
        rule %r/[+-]?\d+/, Num::Integer

        # Quoted strings (rare in HAL but used in echo / save args)
        rule %r/"[^"\n]*"/, Str::Double
        rule %r/'[^'\n]*'/, Str::Single

        # Bare token: signal or component name without dots
        rule %r/[A-Za-z_][\w-]*/, Name

        # Anything else
        rule %r/./, Text
      end
    end
  end
end
