class Mtop < Formula
  desc "Apple Silicon terminal monitor for macOS"
  homepage "https://github.com/lxrzlyr/mtop"
  url "https://github.com/lxrzlyr/mtop/releases/download/v1.3.0/mtop-1.3.0-source.tar.gz"
  sha256 "4a9b3d74d908aabb396aa5202a5eb279a441236467fc7bf5406b444654fd5f3f"
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
