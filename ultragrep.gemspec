$LOAD_PATH.unshift File.expand_path("../lib", __FILE__)
name = "ultragrep"
require "#{name}/version"

Gem::Specification.new name, Ultragrep::VERSION do |s|
  s.summary = "Ultragrep"
  s.authors = ["John Doe"]
  s.email = "john@example.com"
  s.homepage = "https://github.com/grosser/#{name}"
  s.files = `git ls-files lib bin ext`.split("\n")
  s.license = "MIT"
  s.extensions = ["ext/ultragrep/extconf.rb"]
  s.executables = ["ultragrep"]
  key = File.expand_path("~/.ssh/gem-private_key.pem")
  if File.exist?(key)
    s.signing_key = key
    s.cert_chain = ["gem-public_cert.pem"]
  end
end