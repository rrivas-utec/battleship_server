//
// Created by rudri on 11/15/2019.
//

#ifndef BATTLESHIP_SERVER_CONTROLLER_H
#define BATTLESHIP_SERVER_CONTROLLER_H

#include "common_types.h"
#include "player.h"

struct statement_t {
    action_t action;
    text_t token;
    status_t status;
    text_t parameter;
};

using statement_item_t = std::pair<size_t, statement_t>;
using player_list_t = std::vector<std::unique_ptr<player_t>>;
using statement_list_t = std::queue<statement_item_t>;
using path_list_t = std::vector<path_t>;
using winner_t = std::optional<size_t>;

class controller_timer_t {
private:
    size_t frequency_;
    duration_t interval_;
    timer_action_t action_;
public:
    controller_timer_t(size_t frequency, duration_t interval, timer_action_t action, bool auto_start = true);
    void start();
};

class controller_t {
private:
    // attributes
    text_t              columns_;
    size_t              rows_;
    player_list_t       players_;
    statement_list_t    statements_;
    winner_t            winner_;
    // methods
    void            start(const statement_item_t& item);
    void            build(const statement_item_t& item);
    void            attack(const statement_item_t& item);

    // Queue guard.
    mutable std::mutex statements_guard;
public:
    // constructor
    controller_t(size_t rows, std::string_view columns, std::string_view path, std::string_view second);
    // methods
    void            execute();
    void            load_tokens();
    void            save_tokens();
};

#endif //BATTLESHIP_SERVER_CONTROLLER_H
