class Mtop < Formula
  desc "Apple Silicon terminal monitor for macOS"
  homepage "https://github.com/example/mtop"
  url "https://github.com/example/mtop/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "REPLACE_WITH_REAL_SHA256"
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
