#include "PBRTParser.h"

#include "AssetManager.hpp"
#include <thread>

#include "scene.h"

#include "TokenParser.h"

PBRTParser::ParseResult PBRTParser::parse(PBRTSceneBuilder& builder, const std::filesystem::path& path, AssetManager& assetLoader)
{
	std::thread tokenizeThread([&](){
        tokenize(path);
        });
    std::thread parseTokenThread([&]() {
        parseToken(builder,assetLoader);
     });

	tokenizeThread.join();
	parseTokenThread.join();
	return ParseResult::SUCESS;
}

#define NOT_WHITE_SPACE(c) ((c!=32 && c!= 9 && c!= 10 && c!= 11 && c!= 12 && c!=13))
#define IS_WHITE_SPACE(c) (!NOT_WHITE_SPACE(c))

void PBRTParser::nextToken(const char* text, int text_len, int* seek, int* tok_loc, int* tok_len)
{
    char current_mode = 0; //0: nothing found, 1: string found, 2: comment found 3: other token found


    int cur_seek = *seek;


    for (; cur_seek < text_len; cur_seek++) {
        char cur_char = text[cur_seek];
        if (current_mode == 0) {
            //nothing found yet
            if (NOT_WHITE_SPACE(cur_char))
            {
                if (cur_char == '\"') {
                    current_mode = 1;
                    *tok_loc = cur_seek;
                }
                else if (cur_char == '#') {
                    current_mode = 2;
                }
                else if (cur_char == '(' || cur_char == ')' ||
                    cur_char == '[' || cur_char == ']' ||
                    cur_char == '{' || cur_char == '}' ||
                    cur_char == '<' || cur_char == '>') {
                    *tok_loc = cur_seek;
                    *tok_len = 1;
                    cur_seek++;
                    break;
                }
                else {
                    current_mode = 3;
                    *tok_loc = cur_seek;
                }
            }
            continue;
        }else if (current_mode == 1) {
            //found string start
            if (cur_char == '\"') {
                cur_seek++;
                *tok_len = cur_seek - *tok_loc;
                break;
            }
            continue;
        }else if (current_mode == 2) {
            //found comment start
            if (cur_char == 10 || cur_char == 13) {
                current_mode = 0;
            }
            continue;
        }else if (current_mode == 3) {
            //found normal token start
            if (IS_WHITE_SPACE(cur_char) || cur_char == '\"' || cur_char == '#' ||
                cur_char == '(' || cur_char == ')' ||
                cur_char == '[' || cur_char == ']' ||
                cur_char == '{' || cur_char == '}' ||
                cur_char == '<' || cur_char == '>') {
                *tok_len = cur_seek - *tok_loc;
                break;
            }
        }
    }
    *seek = cur_seek;

}

void PBRTParser::tokenize(const std::filesystem::path& path)
{
	if (g_use_mmap) {
		tokenizeMMAP(path);
	}
}

std::string dequote1(const std::string& input) {
    if (input.length() >= 2 && (input.front() == '\'' || input.front() == '"') &&
        (input.back() == '\'' || input.back() == '"')) {
        // Remove the first and last characters
        return input.substr(1, input.length() - 2);
    }
    else {
        // No quotes to remove
        return input;
    }
}

void PBRTParser::tokenizeMMAP(const std::filesystem::path& path)
{
		std::vector<std::pair<MappedFile, int>> handleFileStack;
		openedMappedFile.emplace_back(path);
		handleFileStack.push_back({ openedMappedFile.back(),0 });
		while (!handleFileStack.empty())
		{
			auto& f = handleFileStack.back().first;
			int* cur_seek_ptr = &handleFileStack.back().second;

			while (*cur_seek_ptr != f.size())
			{
				int tok_loc = 0;
				int tok_len = 0;
				nextToken(f.raw(), f.size(), cur_seek_ptr, &tok_loc, &tok_len);
				if (strncmp("Include",f.raw()+tok_loc,tok_len)==0) {
					nextToken(f.raw(), f.size(), cur_seek_ptr, &tok_loc, &tok_len);
					if (tok_len == 0) {
						handleFileStack.pop_back();
						break;
					}
                    auto pbrtFormatPath = dequote1(std::string(f.raw() + tok_loc, tok_len));
                    auto searchDir = path.parent_path();
                    size_t pos = pbrtFormatPath.find('/');
                    while (pos != std::string::npos) {
                        searchDir.append(pbrtFormatPath.substr(0,pos));
                        pbrtFormatPath = pbrtFormatPath.substr(pos + 1);
                        pos = pbrtFormatPath.find('/');
                    }
                    searchDir.append(pbrtFormatPath);

				    openedMappedFile.emplace_back(searchDir);
					handleFileStack.push_back({ openedMappedFile.back(),0 });
					break;
				}
                Token t{ f.raw(),tok_loc,tok_len };
                /*for (int i = 0; i < t.len; i++) {
                    printf("%c", (t.str + t.pos)[i]);
                }
                printf("\n");
                printf("\t [offset : %d \t length : %d\n]", t.pos, t.len);*/
				token_queue.waitAndEnqueue(t);
			}
		}

        token_queue.waitAndEnqueue({ nullptr,0,0 });
}

void PBRTParser::parseToken(PBRTSceneBuilder& builder, AssetManager& assetLoader)
{
	static TokenParser tp;
    tp.parse(builder,token_queue,assetLoader);
    openedMappedFile.clear();
}