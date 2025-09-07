#include <iostream>
#include <string>

int main() {

	std::string nickname;

	while(1)
	{
		std::cout << "Input Nickname: ";
		std::getline(std::cin, nickname);
		if (nickname.lentth() > 0)
			break;
		std::cout << "Didn't Input nickname" << std::endl;
	}
	return 0;
}
