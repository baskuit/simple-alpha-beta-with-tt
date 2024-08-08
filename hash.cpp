#include <iostream>
#include <cstdint>
#include <array>
#include <chrono>

// use lower bits of state as hash/TT index
constexpr int hash_bits {28};
constexpr int hash_bits_comp {64 - hash_bits};
constexpr size_t n_entries{1ULL << hash_bits};

// global TT
struct Entry {
    uint8_t value{};
    uint8_t move{};

    void set(uint64_t value, int move) {
        this->value = static_cast<uint8_t>(value);
        this->move = static_cast<uint8_t>(move);
        this->move &= (1 << 7);
    }

    bool is_set() const {
        return move & (1 << 7);
    }
};

Entry *TT_ptr = new Entry[n_entries];

std::string to_string(uint64_t x) {
    std::string result{};
    result.resize(64, '0');
    for (int i = 0; x > 0; ++i) {
        result[i] = (x & 1) ? '1' : '0';
        x >>= 1;
    }
    return result;
}

// state definition. To keep it simple, the state is a 64 bit which is also basically a hash.
// applying a move is repeated xor_shifting, the number depending on the action
// this means transpositions are bound to happen
template <size_t moves, bool debug = false>
struct State {
    // highest bit is player to move
    uint64_t data;

    constexpr std::array<int, moves> get_moves () const {
        int i = 0;
        std::array<int, moves> actions;
        for (; i < moves; ++i) {
            actions[i] = i;
        }
        return actions;
    }

    bool player_one_move() const {
        return !(data >> 63);
    }

    // Designed to cause transpositions of the state
    void apply_move(const int action) {
        uint64_t next_player_bit = (data >> 63) ^ 1;
        static int diffs[]{2, 3, 5, 7, 11};
        std::cout << "apply move - start: " << to_string(data) << std::endl;
        data = (data << 1) >> 1;
        std::cout << "apply move - no player bit: " << to_string(data) << std::endl;
        for (int i = 0; i < diffs[action]; ++i) {
            data = xorshift(data);
        std::cout << "apply move - shift: " << to_string(data) << std::endl;

        }
        data = data ^ (next_player_bit  << 63);
        std::cout << "apply move - end: " << to_string(data) << std::endl;
        std::cout << std::endl;
    }

    uint64_t value () const {
        return data & 0xFF;
    }

    // expets "63 bit" input aka player to move bit is removed
    uint64_t xorshift(uint64_t x) const {
        x ^= x >> 13;
        x ^= (x << (7 + 1)) >> 1;
        x ^= x >> 17;
        return x;
    }
};



template <typename State, bool use_tt>
uint64_t alpha_beta(const State &state, int& depth_remaining, size_t& count, uint64_t alpha = 0, uint64_t beta = 255) {
    ++count;

    if (depth_remaining == 0) {
        return state.value();
    }

    uint32_t hash; 
    if constexpr (use_tt) {
        hash = (state.data << hash_bits_comp) >> hash_bits_comp;
        if (TT_ptr[hash].is_set()) {
            return TT_ptr[hash].value;
        }
    }

    const auto moves = state.get_moves();
    int best_move = 0;
    uint64_t value;

    const auto go = [&](const int move){
        State state_copy{state};
        state_copy.apply_move(move);
        
        --depth_remaining;
        uint64_t value = alpha_beta<State, use_tt>(state_copy, depth_remaining, count, alpha, beta);
        ++depth_remaining;
    };

    if (state.player_one_move()) {
        for (const int move : moves) {
            go(move);
            if (value >= alpha) {
                alpha = value;
                best_move = move;
            }

            if (beta <= alpha) {
                break;
            }
        }
        value = alpha;
    } else {
        for (const int move : moves) {
            go(move);
            if (value <= beta) {
                beta = value;
                best_move = move;
            }

            if (beta <= alpha) {
                break;
            }
        }
        value = beta;
    }
    
    if constexpr (use_tt) {
     TT_ptr[hash].set(value, best_move);
    }

    return value;
}

constexpr uint64_t initial_state_data = 4923481029348345ULL;

void test () {
    // uint64_t last_index = 1ULL << 32;
    // last_index -= 1;
    // TT_ptr[last_index] = {1, 1};
    // std::cout << "Hi" << std::endl;

    
    State<4> state{initial_state_data};

    std::cout << "Initial State Moves:" << std::endl;
    for (const auto move : state.get_moves()) {
        std::cout << move << ' ';
    }
    std::cout << std::endl;

    size_t count{};
    int depth_remaining{1};
    alpha_beta<State<4>, false>(state, depth_remaining, count);

    int x;
    std::cin >> x;
}


int main () {
    // State<4> state{initial_state_data};
    test();
    return 0;
}

