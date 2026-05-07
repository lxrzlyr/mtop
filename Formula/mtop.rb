class Mtop < Formula
  desc "Apple Silicon terminal monitor for macOS"
  homepage "https://github.com/lxrzlyr/mtop"
  url "https://github.com/lxrzlyr/mtop/releases/download/v1.3.0/mtop-1.3.0-source.tar.gz"
  sha256 "b87d21912cac37695a6f320d1ce784d53b5836c166e0bffeefa9e7a46ed7463c"
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
