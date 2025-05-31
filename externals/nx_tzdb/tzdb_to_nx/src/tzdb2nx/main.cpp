#include "tzif.h"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

constexpr std::size_t ten_megabytes{(1 << 20) * 10};

static void ShortHelp(const char *argv0) {
  std::fprintf(stderr, "Usage: %s [INFILE] [OUTFILE]\n", argv0);
}

static void PrintArg(const char *short_arg, const char *long_arg,
                     const char *text) {
  std::fprintf(stderr, "%5s, %-20s %s\n", short_arg, long_arg, text);
}

static void PrintHelp(const char *argv0) {
  ShortHelp(argv0);
  std::fprintf(stderr,
               "Converts a TZif file INFILE from the RFC8536 format to a "
               "Nintendo Switch compatible file OUTFILE.\nWith no arguments, "
               "tzdb2nx can read and write from stdin/stdout, "
               "respectively.\nGiving no arguments without input will print "
               "usage information and exit the program.\n\nArguments:\n");
  PrintArg("-h", "--help", "Print this help text and exit");
}

int main(int argc, char *argv[]) {
  int f{STDIN_FILENO};
  const char *filename{"(stdin)"};
  std::size_t filesize{ten_megabytes};

  const char *optstring = "h";
  int c;
  const struct option longopts[] = {
      {
          "help",
          no_argument,
          nullptr,
          'h',
      },
      {
          nullptr,
          0,
          nullptr,
          0,
      },
  };

  while ((c = getopt_long(argc, argv, optstring, longopts, nullptr)) != -1) {
    switch (c) {
    case 'h':
      PrintHelp(argv[0]);
      return -1;
    case '?':
      ShortHelp(argv[0]);
      return -1;
    }
  }

  if (argc > 1) {
    filename = argv[1];
    f = open(filename, O_RDONLY);

    if (f == -1) {
      const int err = errno;
      std::fprintf(stderr, "%s: %s\n", filename, std::strerror(err));
      return err;
    }

    struct stat statbuf;
    fstat(f, &statbuf);

    filesize = statbuf.st_size;
  } else {
    struct pollfd fds {
      f, POLLIN, 0,
    };

    const int result = poll(&fds, 1, 0);
    if (result == 0) {
      std::fprintf(stderr, "%s: No input\n", filename);
      ShortHelp(argv[0]);
      return -1;
    }
  }

  u_int8_t *buf = new u_int8_t[filesize];

  filesize = read(f, buf, filesize);
  if (filesize == static_cast<std::size_t>(-1)) {
    const int err = errno;
    std::fprintf(stderr, "%s: %s\n", filename, std::strerror(err));
    return err;
  }
  int result = close(f);
  if (result == -1) {
    const int err = errno;
    std::fprintf(stderr, "%s: %s\n", filename, std::strerror(err));
    return err;
  }

  if (filesize < 4) {
    std::fprintf(stderr, "%s: Too small\n", filename);
    return -1;
  }
  if (std::strncmp(reinterpret_cast<const char *>(buf), "TZif", 4) != 0) {
    std::fprintf(stderr, "%s: Bad magic number\n", filename);
    return -1;
  }

  const std::unique_ptr<Tzif::Data> tzif_data = Tzif::ReadData(buf, filesize);
  if (tzif_data == nullptr) {
    std::fprintf(stderr, "%s: Error occured while reading data\n", filename);
    return -1;
  }

  delete[] buf;

  std::vector<u_int8_t> output_buffer;
  tzif_data->ReformatNintendo(output_buffer);

  filename = "(stdout)";
  f = STDOUT_FILENO;
  if (argc > 2) {
    filename = argv[2];
    f = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664);

    if (f == -1) {
      const int err = errno;
      std::fprintf(stderr, "%s: %s\n", filename, std::strerror(err));
      return err;
    }
  }

  result = write(f, output_buffer.data(), output_buffer.size());
  if (result == -1) {
    const int err = errno;
    std::fprintf(stderr, "%s: %s\n", filename, std::strerror(err));
    return err;
  }

  result = close(f);
  if (result == -1) {
    const int err = errno;
    std::fprintf(stderr, "%s: %s\n", filename, std::strerror(err));
    return err;
  }

  return 0;
}
