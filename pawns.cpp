/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2010 Marco Costalba, Joona Kiiski, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


////
//// Includes
////

#include <cassert>
#include <cstring>

#include "bitcount.h"
#include "pawns.h"
#include "position.h"


////
//// Local definitions
////

namespace {

  /// Constants and variables

  #define S(mg, eg) make_score(mg, eg)

  // Doubled pawn penalty by opposed flag and file
  const Score DoubledPawnPenalty[2][8] = {
  { S(13, 43), S(20, 48), S(23, 48), S(23, 48),
    S(23, 48), S(23, 48), S(20, 48), S(13, 43) },
  { S(13, 43), S(20, 48), S(23, 48), S(23, 48),
    S(23, 48), S(23, 48), S(20, 48), S(13, 43) }};

  // Isolated pawn penalty by opposed flag and file
  const Score IsolatedPawnPenalty[2][8] = {
  { S(37, 45), S(54, 52), S(60, 52), S(60, 52),
    S(60, 52), S(60, 52), S(54, 52), S(37, 45) },
  { S(25, 30), S(36, 35), S(40, 35), S(40, 35),
    S(40, 35), S(40, 35), S(36, 35), S(25, 30) }};

  // Backward pawn penalty by opposed flag and file
  const Score BackwardPawnPenalty[2][8] = {
  { S(30, 42), S(43, 46), S(49, 46), S(49, 46),
    S(49, 46), S(49, 46), S(43, 46), S(30, 42) },
  { S(20, 28), S(29, 31), S(33, 31), S(33, 31),
    S(33, 31), S(33, 31), S(29, 31), S(20, 28) }};

  // Pawn chain membership bonus by file
  const Score ChainBonus[8] = {
    S(11,-1), S(13,-1), S(13,-1), S(14,-1),
    S(14,-1), S(13,-1), S(13,-1), S(11,-1)
  };

  // Candidate passed pawn bonus by rank
  const Score CandidateBonus[8] = {
    S( 0, 0), S( 6, 13), S(6,13), S(14,29),
    S(34,68), S(83,166), S(0, 0), S( 0, 0)
  };

  #undef S
}


////
//// Functions
////

/// PawnInfoTable c'tor and d'tor instantiated one each thread

PawnInfoTable::PawnInfoTable() {

  entries = new PawnInfo[PawnTableSize];

  if (!entries)
  {
      std::cerr << "Failed to allocate " << (PawnTableSize * sizeof(PawnInfo))
                << " bytes for pawn hash table." << std::endl;
      exit(EXIT_FAILURE);
  }
  memset(entries, 0, PawnTableSize * sizeof(PawnInfo));
}


PawnInfoTable::~PawnInfoTable() {

  delete [] entries;
}


/// PawnInfoTable::get_pawn_info() takes a position object as input, computes
/// a PawnInfo object, and returns a pointer to it. The result is also stored
/// in a hash table, so we don't have to recompute everything when the same
/// pawn structure occurs again.

PawnInfo* PawnInfoTable::get_pawn_info(const Position& pos) const {

  assert(pos.is_ok());

  Key key = pos.get_pawn_key();
  unsigned index = unsigned(key & (PawnTableSize - 1));
  PawnInfo* pi = entries + index;

  // If pi->key matches the position's pawn hash key, it means that we
  // have analysed this pawn structure before, and we can simply return
  // the information we found the last time instead of recomputing it.
  if (pi->key == key)
      return pi;

  // Clear the PawnInfo object, and set the key
  memset(pi, 0, sizeof(PawnInfo));
  pi->halfOpenFiles[WHITE] = pi->halfOpenFiles[BLACK] = 0xFF;
  pi->kingSquares[WHITE] = pi->kingSquares[BLACK] = SQ_NONE;
  pi->key = key;

  // Calculate pawn attacks
  Bitboard wPawns = pos.pieces(PAWN, WHITE);
  Bitboard bPawns = pos.pieces(PAWN, BLACK);
  pi->pawnAttacks[WHITE] = ((wPawns << 9) & ~FileABB) | ((wPawns << 7) & ~FileHBB);
  pi->pawnAttacks[BLACK] = ((bPawns >> 7) & ~FileABB) | ((bPawns >> 9) & ~FileHBB);

  // Evaluate pawns for both colors
  pi->value =  evaluate_pawns<WHITE>(pos, wPawns, bPawns, pi)
             - evaluate_pawns<BLACK>(pos, bPawns, wPawns, pi);
  return pi;
}


/// PawnInfoTable::evaluate_pawns() evaluates each pawn of the given color

template<Color Us>
Score PawnInfoTable::evaluate_pawns(const Position& pos, Bitboard ourPawns,
                                    Bitboard theirPawns, PawnInfo* pi) const {
  Bitboard b;
  Square s;
  File f;
  Rank r;
  bool passed, isolated, doubled, opposed, chain, backward, candidate;
  Score value = SCORE_ZERO;
  const BitCountType Max15 = CpuIs64Bit ? CNT64_MAX15 : CNT32_MAX15;
  const Square* ptr = pos.piece_list_begin(Us, PAWN);

  // Loop through all pawns of the current color and score each pawn
  while ((s = *ptr++) != SQ_NONE)
  {
      assert(pos.piece_on(s) == piece_of_color_and_type(Us, PAWN));

      f = square_file(s);
      r = square_rank(s);

      // This file cannot be half open
      pi->halfOpenFiles[Us] &= ~(1 << f);

      // Our rank plus previous one. Used for chain detection.
      b = rank_bb(r) | rank_bb(Us == WHITE ? r - Rank(1) : r + Rank(1));

      // Passed, isolated, doubled or member of a pawn
      // chain (but not the backward one) ?
      passed   = !(theirPawns & passed_pawn_mask(Us, s));
      doubled  =   ourPawns   & squares_in_front_of(Us, s);
      opposed  =   theirPawns & squares_in_front_of(Us, s);
      isolated = !(ourPawns   & neighboring_files_bb(f));
      chain    =   ourPawns   & neighboring_files_bb(f) & b;

      // Test for backward pawn
      //
      backward = false;

      // If the pawn is passed, isolated, or member of a pawn chain
      // it cannot be backward. If can capture an enemy pawn or if
      // there are friendly pawns behind on neighboring files it cannot
      // be backward either.
      if (   !(passed | isolated | chain)
          && !(ourPawns & attack_span_mask(opposite_color(Us), s))
          && !(pos.attacks_from<PAWN>(s, Us) & theirPawns))
      {
          // We now know that there are no friendly pawns beside or behind this
          // pawn on neighboring files. We now check whether the pawn is
          // backward by looking in the forward direction on the neighboring
          // files, and seeing whether we meet a friendly or an enemy pawn first.
          b = pos.attacks_from<PAWN>(s, Us);

          // Note that we are sure to find something because pawn is not passed
          // nor isolated, so loop is potentially infinite, but it isn't.
          while (!(b & (ourPawns | theirPawns)))
              Us == WHITE ? b <<= 8 : b >>= 8;

          // The friendly pawn needs to be at least two ranks closer than the enemy
          // pawn in order to help the potentially backward pawn advance.
          backward = (b | (Us == WHITE ? b << 8 : b >> 8)) & theirPawns;
      }

      assert(passed | opposed | (attack_span_mask(Us, s) & theirPawns));

      // Test for candidate passed pawn
      candidate =   !(opposed | passed)
                 && (b = attack_span_mask(opposite_color(Us), s + pawn_push(Us)) & ourPawns) != EmptyBoardBB
                 &&  count_1s<Max15>(b) >= count_1s<Max15>(attack_span_mask(Us, s) & theirPawns);

      // Mark the pawn as passed. Pawn will be properly scored in evaluation
      // because we need full attack info to evaluate passed pawns. Only the
      // frontmost passed pawn on each file is considered a true passed pawn.
      if (passed && !doubled)
          set_bit(&(pi->passedPawns[Us]), s);

      // Score this pawn
      if (isolated)
          value -= IsolatedPawnPenalty[opposed][f];

      if (doubled)
          value -= DoubledPawnPenalty[opposed][f];

      if (backward)
          value -= BackwardPawnPenalty[opposed][f];

      if (chain)
          value += ChainBonus[f];

      if (candidate)
          value += CandidateBonus[relative_rank(Us, s)];
  }
  return value;
}
