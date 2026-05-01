class Mtop < Formula
  desc "Apple Silicon terminal monitor for macOS"
  homepage "https://github.com/lxrzlyr/mtop"
  url "https://github.com/lxrzlyr/mtop/releases/download/v1.0.1/mtop-1.0.1-source.tar.gz"
  sha256 "d73e5bc8bea54f360bb83994b04d5904aa7392237d7ab7fd9a4f658075853013"
  license "GPL-3.0-or-later"

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build"
    system "cmake", "--build", "build"
    system "cmake", "--install", "build", "--prefix", prefix
  end

  test do
    system "#{bin}/mtop", "--demo"
  end
end
