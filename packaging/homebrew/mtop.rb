class Mtop < Formula
  desc "Apple Silicon terminal monitor for macOS"
  homepage "https://github.com/lxrzlyr/mtop"
  url "https://github.com/lxrzlyr/mtop/releases/download/v1.0.0/mtop-1.0.0-source.tar.gz"
  sha256 "79666219551ee5e300d19c32048d22a17b80c8ee5c8b315f762e1dff7eda26b2"
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
