#include "dc_content.h"

#include "dc_common_error.h"
#include "dc_common_assert.h"

#include <openssl/sha.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <mutex>

std::pair<std::vector<std::string>, int>
compute_file_sha1_skip_empty_line(std::string file_path)
{
    std::ifstream ifile(file_path);
    std::string line;
    uint64_t n_line_nr = 0;
    std::string sha1(SHA_DIGEST_LENGTH, 0);
    int empty_lines = 0;
    std::vector<std::string> lines_sha1;

    if (!ifile.is_open()) {
        printf("Failed to open %s\n", file_path.c_str());
        return { lines_sha1, empty_lines};
    }

    // open a file for output
    std::ofstream ofile("/tmp/std_getline.txt");
    if (!ofile.is_open()) {
        printf("Failed to open /tmp/std_getline.txt\n");
        return { lines_sha1, empty_lines};
    }

    while (std::getline(ifile, line)) {
        n_line_nr++;

        if (line.length() == 0) {
            empty_lines++;
            continue;
        }

        int valid_char_number = 0;
        for (auto c: line) {
            if (!isspace(c)) {
                valid_char_number++;
                break;
            }
        }

        if (valid_char_number == 0) {
            empty_lines++;
            continue;
        }

        DC_COMMON_ASSERT(valid_char_number > 0);

        ofile << line.length() << std::endl;

        // use line to compute sha1
        SHA1((const unsigned char*)line.c_str(), line.length(), (unsigned char*)&sha1[0]);

        lines_sha1.push_back(sha1);
        line.clear();
    }

    ifile.close();
    ofile.close();

    return {lines_sha1, empty_lines};
}

std::pair<std::vector<std::string>, int>
compute_file_sha1_skip_empty_line2(std::string file_path)
{
    dc_content_t *content = new dc_content_local_t(NULL);
    dc_file_attr_t attr;
    std::vector<std::string> lines_sha1;
    std::vector<std::string> total_ret;
    int empty_lines = 0;

    // try to get file content of sha1
    auto ret = content->do_file_content(file_path, &lines_sha1, &empty_lines);
    DC_COMMON_ASSERT(ret == S_SUCCESS);

    while (true) {
        ret = content->get_file_content();
        if (ret == E_DC_CONTENT_RETRY) {
            continue;
        } else if (ret == S_SUCCESS || ret == E_DC_CONTENT_OVER) {
            for (auto &line: lines_sha1) {
                total_ret.emplace_back();
                total_ret.back().swap(line);
            }
            lines_sha1.clear();
            if (ret == E_DC_CONTENT_OVER) {
                break;
            }
        } else {
            DC_COMMON_ASSERT(0);
        }
    }

    delete content;

    return {total_ret, empty_lines};
}

std::string
random_generate_big_files(const int lines, const uint64_t total_bytes)
{
    uint64_t used_bytes = 0;
    std::string file_path("/mnt/big_file.txt");
    std::ofstream ofile(file_path);
    if (!ofile.is_open()) {
        printf("Failed to open %s\n", file_path.c_str());
        return "";
    }

    for (int i = 0; i < lines || used_bytes < total_bytes; i++) {
        // random generate a line which length < 4096
        int line_len = rand() % 4096;
        for (int j = 0; j < line_len; j++) {
            ofile << (char)(rand() % 256);
            used_bytes++;
        }
        ofile << std::endl;
    }

    ofile.close();

    return file_path;
}

int
TEST_check_single_file(std::string file_path)
{
    // use stat() to file attr,
    // and check if file is regular file
    struct stat file_stat;
    if (stat(file_path.c_str(), &file_stat) != 0) {
        //printf("Failed to stat %s error:%d error_str:%s\n",
        //       file_path.c_str(),
        //       errno,
        //       strerror(errno));
        return 0;
    }

    if (!S_ISREG(file_stat.st_mode)) {
        //printf("%s is not a regular file\n", file_path.c_str());
        return 0;
    }

    auto sync = compute_file_sha1_skip_empty_line(file_path);
    auto async = compute_file_sha1_skip_empty_line2(file_path);

    if (sync.first.size() != async.first.size()) {
        printf("sync %lu async %lu\n",
               sync.first.size(),
               async.first.size());
    }
    DC_COMMON_ASSERT(sync.first.size() == async.first.size());

    const int N = sync.first.size();
    for (int i = 0; i < N; i++) {
        DC_COMMON_ASSERT(sync.first[i] == async.first[i]);
    }
    DC_COMMON_ASSERT(sync.second == async.second);

    return 0;
}

int
main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s file_path\n", argv[0]);
        return 0;
    }

    std::string file_path(argv[1]);
    TEST_check_single_file(file_path);

    for (int line = 1; line < 1000000; line *= 10) {
        const uint64_t total_bytes = rand() % 1024 * 1024 * 1024 + 1;
        printf("line %d total_bytes %lu\n", line, total_bytes);
        file_path = random_generate_big_files(line, total_bytes);
        TEST_check_single_file(file_path);
    }

    return 0;
}
