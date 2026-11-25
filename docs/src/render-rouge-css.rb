#!/usr/bin/env ruby
# Render the Rouge github theme as CSS scoped to .rouge spans.  Light
# variant first, dark variant gated on prefers-color-scheme.  Older
# Rouge releases lack the light!/dark! mode switch, so we fall back to
# Monokai for the dark cut and warn.
require "rouge"

t = Rouge::Themes::Github
if t.respond_to?(:light!) && t.respond_to?(:dark!)
  t.light!
  puts t.new(scope: ".rouge").render
  puts "@media (prefers-color-scheme: dark) {"
  t.dark!
  puts t.new(scope: ".rouge").render
  puts "}"
else
  warn "rouge does not support theme modes (Rouge::Themes::Github.light!); dark mode code highlighting will use Monokai"
  puts t.new(scope: ".rouge").render
  puts "@media (prefers-color-scheme: dark) {"
  puts Rouge::Themes::Monokai.new(scope: ".rouge").render
  puts "}"
end
