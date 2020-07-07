//
// Created by rudri on 11/15/2019.
//

#include "controller.h"
using namespace std::string_literals;

template <typename integer_t,
	typename = std::enable_if_t<std::is_integral<integer_t>::value >
>
integer_t rand_int(integer_t first, integer_t last) {
	std::random_device dev;
	std::mt19937_64 eng(dev());
	std::uniform_int_distribution<integer_t> gen{ first, last };
	return gen(eng);
}

location_t get_coordinates(std::string value)
{
	std::regex r(R"(^([A-Z])(\d+)$)");
	auto mit = std::sregex_iterator(begin(value), end(value), r);
	auto end = std::sregex_iterator();
	if (mit == end)
		throw std::runtime_error(value + " is not well formatted");

	auto sit = begin(*mit);
	text_t col_ = (++sit)->str();
	text_t row_ = (++sit)->str();

	auto ucol_ = static_cast<size_t>(col_[0]) - 'A';
	auto urow_ = static_cast<size_t>(std::stoul(row_) - 1u);

	return { ucol_, urow_ };
}

statement_t push_statement(const path_t& file_name)
{
	using namespace std::chrono_literals;
	std::ifstream file_;
	file_.open(file_name.generic_string());
	if (!file_.is_open()) {
		controller_timer_t timer(2, 4ms, [&]() {
			file_.open(file_name.generic_string()); 
			return file_.is_open();
			});
	}
	file_.exceptions(std::ifstream::failbit | std::ifstream::badbit);

	text_t line_;
	std::getline(file_, line_);
	std::stringstream first_(line_);

	text_t key_;
	text_t value_;
	std::getline(first_, key_, '=');
	std::getline(first_, value_);

	statement_t result{};
	if (key_ == "HANDSHAKE") {
		result.action = action_t::start;
		result.parameter = value_;
		result.status = status_t::ok;
	}
	else if (key_ == "TOKEN") {
		result.token = value_;
		std::getline(file_, line_);
		std::stringstream second_(line_);
		std::getline(second_, key_, '=');
		std::getline(second_, value_);
		if (key_ == "PLACEFLEET")
			result.action = action_t::build;
		else if (key_ == "ATTACK")
			result.action = action_t::attack;
		result.status = status_t::ok;
		result.parameter = value_;
	}
	return result;
}

void controller_t::load_tokens()
{
	std::cout << "start load tokens" << std::endl;
	auto end_ = filesystem::directory_iterator{};
	std::error_code e;
	while (true) {
		try {
			filesystem::directory_iterator first_{ players_[0]->get_path() / "in"s };
			filesystem::directory_iterator second_{ players_[1]->get_path() / "in"s };
			while (first_ != end_ || second_ != end_) {
				if (first_ != end_) {
					if (first_->path().extension() == ".in"s) {
						statements_.push({ 0u, push_statement(*first_) });
						filesystem::remove(*first_, e);
						if (e)
							std::cerr << e.message() << "\n";
					}
					first_++;
				}
				if (second_ != end_) {
					if (second_->path().extension() == ".in"s) {
						statements_.push({ 1u, push_statement(*second_) });
						filesystem::remove(*second_, e);
						if (e)
							std::cerr << e.message() << "\n";
					}
					second_++;
				}
			}
		}
		catch (const std::ios_base::failure& e) {
			std::cerr << "file is used by another process..." << std::endl;
		}
		catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
		}
	}
}

void controller_t::save_tokens()
{
	auto end_ = filesystem::directory_iterator{};
	while (true) {
		try {
			while (!statements_.empty()) {
				auto item_ = statements_.front();
				statements_.pop();
				switch (item_.second.action) {
				case action_t::start:
					start(item_);
					break;
				case action_t::build:
					build(item_);
					break;
				case action_t::attack:
					attack(item_);
					break;
				}
			}
		}
		catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
		}
	}
}

void controller_t::start(const statement_item_t& item)
{
	auto& player_ = players_[item.first];
	text_t status_;
	text_t message_;
	if (player_->get_id() != 0) {
		status_ = "REJECTED";
		std::cout << "player " << player_->get_name() << " was assigned previously with: " << player_->get_id() << "\n";
		message_ = "HANDSHAKE WAS DONE BEFORE";
	}
	else {
		status_ = "ACCEPTED";
		message_ = "CONTINUE";
		auto& another_ = players_[item.first == 0u ? 1u : 0u];
		do {
			player_->set_id(rand_int(100000u, 999999u));
		} while (player_->get_id() == another_->get_id());

		player_->set_name(item.second.parameter);
		std::cout << "player " << player_->get_name() << " has been assigned with: " << player_->get_id() << "\n";
	}

	text_t action_ = "HANDSHAKE";
	size_t token_ = player_->get_id();
	text_t scope_ = columns_ + "-" + std::to_string(rows_);

	auto file_name_ = player_->get_prefix() + std::to_string(player_->sequence()++) + ".out";
	std::ofstream file(player_->get_path() / "out"s / file_name_);
	file << action_ << "\n";
	file << "STATUS=" << status_ << "\n";
	file << "TOKEN=" << token_ << "\n";
	file << "SCOPE=" << scope_ << "\n";
	file << "MESSAGE=" << message_ << "\n";

}

void controller_t::build(const statement_item_t& item)
{
	auto& player_ = players_[item.first];
	text_t status_ = "ACCEPTED";
	text_t message_;
	if (player_->get_id() == 0) {
		message_ = "HANDCHECK MISSED";
		status_ = "REJECTED";
	}
	else if (player_->get_id() != std::stoul(item.second.token)) {
		message_ = "ID DOES NOT MATCH";
		status_ = "REJECTED";
	}
	else {
		std::stringstream ss(item.second.parameter);
		text_t model_;
		text_t slocation_;
		text_t orientation_;
		std::getline(ss, model_, '-');
		std::getline(ss, slocation_, '-');
		std::getline(ss, orientation_);
		auto location_ = get_coordinates(slocation_);
		auto add_status_ = player_->add_navy(location_, model_t(model_[0]),
			orientation_t(orientation_[0]), { {0, 0}, {static_cast<length_t>(columns_[0]) - 'A' + 1, rows_} });

		if (add_status_ == status_t::busy) {
			status_ = "REJECTED";
			message_ = "BUSY";
		}
		else if (add_status_ == status_t::fleet_full) {
			status_ = "REJECTED";
			message_ = "FULL";
		}
		else if (add_status_ == status_t::model_full) {
			status_ = "REJECTED";
			message_ = "COMPLETE";
		}
		else if (add_status_ == status_t::outside) {
			status_ = "REJECTED";
			message_ = "OUTSIDE";
		}
		else if (add_status_ == status_t::ok) {
			status_ = "ACCEPTED";
			message_ = "CONTINUE";
		}
	}

	text_t action_ = "PLACEFEET";
	auto file_name_ = player_->get_prefix() + std::to_string(player_->sequence()++) + ".out";
	std::ofstream file(player_->get_path() / "out"s / file_name_);
	file << action_ << "\n";
	file << "STATUS=" << status_ << "\n";
	file << "MESSAGE=" << message_ << "\n";
	std::cout << "ID=" << item.second.token << " ACTION=" << action_ << " STATUS=" << status_ << " MESSAGE=" << message_ << "\n";
}

void controller_t::attack(const statement_item_t& item)
{
	auto& player_ = players_[item.first];
	text_t status_ = "ACCEPTED";
	text_t message_;
	if (player_->get_id() == 0) {
		message_ = "HANDCHECK MISSED";
		status_ = "REJECTED";
	}
	else if (player_->get_id() != std::stoul(item.second.token)) {
		message_ = "ID DOES NOT MATCH";
		status_ = "REJECTED";
	}
	else {
		auto location_ = get_coordinates(item.second.parameter);
		auto& opponent_ = players_[item.first == 0u ? 1u : 0u];
		auto hit_result_ = opponent_->hit_navy(location_);

		if (opponent_->get_fleet_size() == 0) {
			std::cout << "player #" << item.first + 1 << " cannot attack if opponent does not have any navy located\n";
			status_ = "REJECTED";
			message_ = "NO OPPONENT";
		}
		else if (!player_->is_fleet_full()) {
			std::cout << "player #" << item.first + 1 << " cannot attack if fleet has not been completed\n";
			status_ = "REJECTED";
			message_ = "FLEET INCOMPLETE";
		}
		else if (opponent_->is_fleet_destroyed()) {
			if (winner_.has_value()) {
				if (winner_ == item.first)
					message_ = "WINNER";
				else
					message_ = "GAMEOVER";
			}
			else {
				winner_ = item.first;
				message_ = "WINNER";
			}
		}
		else if (!hit_result_.second) {
			message_ = "FAILED";
		}
		else if (hit_result_.first->get_status() == navy_status_t::damaged) {
			message_ = "DAMAGED";
		}
		else if (hit_result_.first->get_status() == navy_status_t::destroyed) {
			message_ = "DESTROYED";
		}
	}

	auto file_name_ = player_->get_prefix() + std::to_string(player_->sequence()++) + ".out";
	std::ofstream file(player_->get_path() / "out"s / file_name_);
	text_t action_ = "ATTACK";
	file << action_ << "\n";
	file << "STATUS=" << status_ << "\n";
	file << "MESSAGE=" << message_ << "\n";
	std::cout << "ID=" << item.second.token << " ACTION=" << action_ << " STATUS=" << status_ << " MESSAGE=" << message_ << "\n";
}

controller_t::controller_t(size_t rows, std::string_view columns,
	std::string_view first, std::string_view second) : columns_{ columns }, rows_{ rows }
{
	players_.push_back(std::make_unique<player_t>(filesystem::current_path() / first, "FirstPlayer"));
	players_.push_back(std::make_unique<player_t>(filesystem::current_path() / second, "SecondPlayer"));
	for (const auto& p : players_) {
		if (!filesystem::exists(p->get_path() / "in"))
			filesystem::create_directories(p->get_path() / "in");
		if (!filesystem::exists(p->get_path() / "out"))
			filesystem::create_directories(p->get_path() / "out");
	}
}

void controller_t::execute()
{
	auto load_ = std::async([&] { load_tokens(); });
	auto save_ = std::async([&] { save_tokens(); });
}

controller_timer_t::controller_timer_t(size_t frequency, duration_t interval, timer_action_t action, bool auto_start) :
	frequency_{ frequency }, interval_{ interval }, action_{ action } {
	if (auto_start) start();
}

void controller_timer_t::start() {
	auto n = frequency_;
	while (--n) {
		std::this_thread::sleep_for(interval_);
		if (action_()) break;
	}
}

