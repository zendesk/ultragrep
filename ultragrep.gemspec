$LOAD_PATH.unshift File.expand_path("../lib", __FILE__)
name = "ultragrep"
require "#{name}/version"

Gem::Specification.new name, Ultragrep::VERSION do |s|
  s.summary = "Ultragrep"
  s.authors = ["Ben Osheroff"]
  s.email = "ben@zendesk.com"
  s.homepage = "https://github.com/zendesk/ultragrep"
  s.files = Dir["{lib,bin,ext,src}/**/*"]
  s.license = 'Apache License Version 2.0'
  s.extensions = ["src/extconf.rb"]
  s.executables = ["ultragrep"]
end
