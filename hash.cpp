#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>

// use lower bits of state as hash/TT index
constexpr int hash_bits{28};
constexpr int hash_bits_comp{64 - hash_bits};
constexpr size_t n_entries{1ULL << hash_bits};

// global TT
struct Entry {
  uint8_t value{};
  uint8_t move{};

  void set(uint64_t value, int move) {
    this->value = static_cast<uint8_t>(value);
    this->move = static_cast<uint8_t>(move);
    this->move |= (1 << 7);
  }

  bool is_set() const { return move & (1 << 7); }
};

Entry *TT_ptr = new Entry[n_entries];

std::string to_string(uint64_t x) {
  std::string result{};
  result.resize(64, '0');
  for (int i = 63; x > 0; --i) {
    result[i] = (x & 1) ? '1' : '0';
    x >>= 1;
  }
  return result;
}

// state definition. To keep it simple, the state is a 64 bit which is also
// basically a hash. applying a move is repeated xor_shifting, the number
// depending on the action this means transpositions are bound to happen
template <size_t moves, bool debug = false> struct State {
  // highest bit is player to move
  uint64_t data;

  constexpr std::array<int, moves> get_moves() const {
    int i = 0;
    std::array<int, moves> actions;
    for (; i < moves; ++i) {
      actions[i] = i;
    }
    return actions;
  }

  // player one to move if most sig bit is 0
  bool player_one_to_move() const { return !(data >> 63); }

  // Designed to cause transpositions of the state
  void apply_move(const int action) {
    uint64_t next_player_bit = (data >> 63) ^ 1;
    const int offset = player_one_to_move() ? 0 : 1;

    static int diffs[]{2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
    // std::cout << to_string(data) << " :start - apply move" << std::endl;
    data = (data << 1) >> 1;
    // std::cout << to_string(data) << " :no player bit - apply move" <<
    // std::endl;
    const int index = 2 * action + offset;
    for (int i = 0; i < diffs[index]; ++i) {
      data = xorshift(data);
      //   std::cout << to_string(data) << " :shift - apply move" << std::endl;
    }
    data = b(data);
    data = data ^ (next_player_bit << 63);
    // std::cout << to_string(data) << " :end - apply move" << std::endl;
    // std::cout << std::endl;
  }

  uint64_t value() const { return data & 0xFF; }

  // expets "63 bit" input aka player to move bit is removed
  uint64_t xorshift(uint64_t x) const {
    x ^= x >> 13;
    x ^= (x << (7 + 1)) >> 1;
    x ^= x >> 17;
    return x;
  }

  uint64_t b(uint64_t x) const {
    x ^= x >> 12;
    x ^= (x << (25 + 1)) >> 1;
    x ^= x >> 27;
    return x;
  }
};

struct BaseData {
  BaseData(int depth_remaining) : depth_remaining{depth_remaining} {}

  int depth_remaining;
  size_t count{};
  size_t cache_hits{};
};

// self explanatory - just regular alpha beta, very easy to implement
template <typename State, bool use_tt>
uint64_t alpha_beta(const State &state, BaseData &base_data, uint64_t alpha = 0,
                    uint64_t beta = 255) {
  int &depth_remaining = base_data.depth_remaining;
  size_t &count = base_data.count;
  size_t &cache_hits = base_data.cache_hits;

  if (depth_remaining == 0) {
    return state.value();
  }

  ++count;

  uint32_t hash;
  if constexpr (use_tt) {
    hash = (state.data << hash_bits_comp) >> hash_bits_comp;

    if (TT_ptr[hash].is_set()) {
      ++cache_hits;
      return TT_ptr[hash].value;
    }
  }

  const auto moves = state.get_moves();
  int best_move = 0;
  uint64_t value;

  const auto go = [&](const int move) {
    State state_copy{state};
    state_copy.apply_move(move);

    --depth_remaining;
    value = alpha_beta<State, use_tt>(state_copy, base_data, alpha, beta);
    ++depth_remaining;
  };

  if (state.player_one_to_move()) {
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

void test() {
  State<4> state{initial_state_data};
  auto state_a{state};
  auto state_b{state};

  state_a.apply_move(0);
  state_a.apply_move(1);
  state_b.apply_move(1);
  state_b.apply_move(0);

  std::cout << state_a.data << std::endl;
  std::cout << state_b.data << std::endl;
}

void search() {
  State<4> state{initial_state_data};

  const int max_depth = 15;
  BaseData base_data{max_depth};

  const auto root_value = alpha_beta<State<4>, true>(state, base_data);

  std::cout << "depth: " << base_data.depth_remaining << std::endl;
  std::cout << "count: " << base_data.count << std::endl;
  std::cout << "cache hits: " << base_data.cache_hits << std::endl;
  std::cout << "root_value: " << root_value << std::endl;
  std::cout << "root value estimate: " << state.value() << std::endl;
}

int main() {
  //   test();
  search();
  return 0;
}
