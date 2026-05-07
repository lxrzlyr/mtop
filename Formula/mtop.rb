class Mtop < Formula
  desc "Apple Silicon terminal monitor for macOS"
  homepage "https://github.com/lxrzlyr/mtop"
  url "https://github.com/lxrzlyr/mtop/releases/download/v1.2.0/mtop-1.2.0-source.tar.gz"
  sha256 "54537a4b654f2aa0c843a96bd0aeeaa5ee807c0b5a5d2158a5ee6d8f4c9434a5"
  license "GPL-3.0-or-later"

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build"
    system "cmake", "--build", "build"
    system "cmake", "--install", "build", "--prefix", prefix
  end

  test do
    system "#{bin}/mtop", "--version"
  end
end
