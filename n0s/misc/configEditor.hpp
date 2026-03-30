#pragma once

#include <fstream>
#include <regex>
#include <streambuf>
#include <string>

#include "../version.hpp"

namespace n0s
{

struct configEditor
{
	std::string m_fileContent;

	configEditor()
	{
	}

	static bool file_exist(const std::string filename)
	{
		std::ifstream fstream(filename);
		return fstream.good();
	}

	void set(const std::string&& content)
	{
		m_fileContent = content;
	}

	bool load(const std::string filename)
	{
		std::ifstream fstream(filename);
		m_fileContent = std::string(
			(std::istreambuf_iterator<char>(fstream)),
			std::istreambuf_iterator<char>());
		return fstream.good();
	}

	void write(const std::string filename)
	{
		// Strip platform endmarks from config templates
		// Drop lines with Windows endmarks, keep Linux lines
		replace(".*---WINDOWS\n", "");
		replace("---LINUX\n", "\n");

		replace("N0S_VERSION", get_version_str());
		std::ofstream out(filename);
		out << m_fileContent;
		out.close();
	}

	void replace(const std::string search, const std::string substring)
	{
		m_fileContent = std::regex_replace(m_fileContent, std::regex(search), substring);
	}
};

} // namespace n0s
