/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <vector>
#include <string>
#include <iostream>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	bool showme = false;
	bool commanding = true;
	bool compiling = true;
	bool linking = true;

	std::vector<std::string> args(argv, argv + argc);

	for (auto i = args.begin(); i != args.end(); ++i) {
		if (*i == "--showme") {
			showme = true;
			commanding = true;
			compiling = true;
			linking = true;
			args.erase(i);
			break;
		}
		if (*i == "--showme:compile") {
			showme = true;
			commanding = false;
			compiling = true;
			linking = false;
			args.erase(i);
			break;
		}
		if (*i == "--showme:link") {
			showme = true;
			commanding = false;
			compiling = false;
			linking = true;
			args.erase(i);
			break;
		}
		if (*i == "--showme:command") {
			showme = true;
			commanding = true;
			compiling = false;
			linking = false;
			args.erase(i);
			break;
		}
	}

	std::string prefix = std::string(args[0], 0, args[0].rfind("bin/"));

	std::vector<std::string> command = { "gcc" };
	std::vector<std::string> compile = { "-pthread", "-I" + prefix + "include" };
	std::vector<std::string> link = { "-Wl,-rpath," + prefix + "lib", "-L" + prefix + "lib", "-lumpi" };

	std::vector<std::string> new_args;
	if (commanding)
		new_args.insert(new_args.end(), command.begin(), command.end());
	new_args.insert(new_args.end(), args.begin() + 1, args.end());
	if (compiling)
		new_args.insert(new_args.end(), compile.begin(), compile.end());
	if (linking)
		new_args.insert(new_args.end(), link.begin(), link.end());

	if (showme) {
		for (auto i = new_args.begin(); i != new_args.end(); ++i) {
			std::cout << *i;
			if (std::next(i) != new_args.end())
				std::cout << " ";
		}
		std::cout << std::endl;
		return 0;
	}

	std::vector<const char *> tmp_args;
	for (auto &a: new_args)
		tmp_args.push_back(a.data());
	tmp_args.push_back(nullptr);

	execvp(tmp_args[0], const_cast<char *const *>(tmp_args.data()));
	perror(tmp_args[0]);
	return 1;
}

