#include "helpers.h"

vector<char> readFile(const std::string& filename) {
	ifstream file(filename, ios::ate | ios::binary);

	if (!file.is_open()) {
		throw runtime_error("ERROR - Failed to open file !");
	}

	size_t fileSize = (size_t)file.tellg();
	vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	return buffer;
}