# docs/src/extensions/rouge_ngc.rb
#
# Rouge lexer for LinuxCNC NGC (RS-274/EMC dialect G-code) files.
# Ported from docs/src/source-highlight/ngc.lang (Michael Haberler 2011,
# originally adapted from Jan Van Gilsen's gedit highlight definition).
#
# Highlights:
#   - line numbers           N123
#   - G / M / T / H / F / S codes
#   - axis letters           X Y Z A B C U V W  (Name::Attribute)
#   - parameter letters      I J K P Q R L D E  (Name::Decorator)
#   - numeric, named, and indexed parameters  #1  #5421  #<varname>
#   - LinuxCNC system parameters  #<_ini[SECTION]NAME>  /  #<_hal[pin.name]>
#   - O-word blocks with their keywords (sub, while, if, call, ...)
#   - math functions and boolean operators
#   - comments: ( ... )   ; rest-of-line   (DEBUG, ...)
#
# Loaded via the asciidoctor -r flag from the docs Submakefile.

require 'rouge'

module Rouge
  module Lexers
    class NGC < RegexLexer
      title 'NGC'
      desc 'RS-274 G-code, LinuxCNC dialect'
      tag 'ngc'
      aliases 'gcode', 'rs274ngc'
      filenames '*.ngc', '*.nc', '*.tap'
      mimetypes 'text/x-ngc'

      MATH = %w[
        cos sin tan acos asin atan
        exp ln sqrt
        fup fix round
        abs exists
      ].join('|').freeze

      BOOL = %w[and or xor not mod gt lt ge le eq ne].join('|').freeze

      OWORD = %w[
        sub endsub return
        if elseif else endif
        while endwhile do break continue
        repeat endrepeat
        call
      ].join('|').freeze

      identifier = /[a-zA-Z_][a-zA-Z0-9_]*/

      # This definition limits hal pin names to minimum 2 characters.
      # Should be no problem.
      halpinname = /[a-zA-Z][a-zA-Z0-9_.-]*[a-zA-Z0-9]/

      state :root do
        rule %r/\s+/, Text

        # Line numbers (N123 at start of statement)
        rule %r/(?<=^|\s)[nN]\d+/, Comment::Preproc

        # Comments
        rule %r/;.*$/, Comment::Single
        rule %r/\([dD][eE][bB][uU][gG],/, Comment::Special, :debug_comment
        rule %r/\([^)]*\)/, Comment::Multiline

        # LinuxCNC system parameters that read live values from outside
        # the gcode scope.  These are very LinuxCNC-flavoured so they
        # get a dedicated token split that highlights the section /
        # pin name distinctly from the surrounding parameter wrapper:
        #
        #   #<_ini[SECTION]NAME>   - value of NAME in [SECTION] of the INI
        #   #<_hal[pin.or.signal]> - current value of a HAL pin or signal
        rule %r/(#<)(_ini)(\[)(#{identifier})(\])(#{identifier})(>)/i do
          groups Punctuation, Name::Builtin, Punctuation,
                 Name::Namespace, Punctuation, Name::Property, Punctuation
        end
        rule %r/(#<)(_hal)(\[)(#{halpinname})(\])(>)/i do
          groups Punctuation, Name::Builtin, Punctuation,
                 Name::Namespace, Punctuation, Punctuation
        end

        # Parameters (named / numbered, generic)
        rule %r/#<[^>]+>/, Name::Variable
        rule %r/#\d+/, Name::Variable

        # O-word lines: O<name> sub  or  O100 if
        rule %r/(?i:[oO])(?:\d+|<[A-Za-z_][\w-]*>)/, Name::Label
        rule %r/(?i:#{OWORD})\b/, Keyword::Reserved

        # Math + boolean operator names
        rule %r/(?i:#{MATH})\b/, Name::Builtin
        rule %r/(?i:#{BOOL})\b/, Operator::Word

        # G / M codes (G33.1, G92.2, M62, ...)
        rule %r/(?i:g)\d+(?:\.\d+)?/, Keyword
        rule %r/(?i:m)\d+/, Keyword

        # T H F S codes
        rule %r/(?i:[tfsh])-?\d+(?:\.\d+)?/, Keyword

        # Axis letters X Y Z A B C U V W followed by a value or expression
        rule %r/(?i:[xyzabcuvw])(?=\s*[-+\[\d#])/, Name::Attribute

        # Argument / parameter letters I J K L P Q R D E
        # (call-by-name arguments, arc centres, dwell times, etc.).
        # Highlighted differently from the axes so users can see at a
        # glance which letters move the tool vs. which configure the move.
        rule %r/(?i:[ijklpqrde])(?=\s*[-+\[\d#])/, Name::Decorator

        # Arithmetic / brackets
        rule %r{[+\-*/=]}, Operator
        rule %r/[\[\]]/, Punctuation

        # Numbers: floats must have a decimal point or an exponent;
        # integers are decimal only (LinuxCNC's RS-274 dialect does not
        # accept hex or other radixes for numeric literals).
        rule %r/[+-]?\d+\.\d*(?:[eE][+-]?\d+)?/, Num::Float
        rule %r/[+-]?\.\d+(?:[eE][+-]?\d+)?/, Num::Float
        rule %r/[+-]?\d+[eE][+-]?\d+/, Num::Float
        rule %r/[+-]?\d+/, Num::Integer

        rule %r/./, Text
      end

      # (DEBUG, ...): same as a comment, but parameter references inside
      # should still be highlighted as variables (DEBUG expands them at
      # run time).
      state :debug_comment do
        rule %r/\)/, Comment::Special, :pop!
        rule %r/#<[^>]+>/, Name::Variable
        rule %r/#\d+/, Name::Variable
        rule %r/[^)#]+/, Comment::Special
      end
    end
  end
end
