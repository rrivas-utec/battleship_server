#include <iostream>
#include "player.h"
#include "controller.h"

const std::map<navy_status_t, std::string> status{
	{navy_status_t::healthy, "healthy"},
	{navy_status_t::damaged, "damaged"},
	{navy_status_t::destroyed, "destroyed"}
};

int main() {
    controller_t ctrl(10, "J", "first", "second");
    ctrl.execute();
	return 0;
}