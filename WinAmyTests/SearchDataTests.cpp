#include "TestHelpers.h"
#include "heap.h"
#include "random.h"
#include "searchdata.h"

#include <memory>
#include <sstream>
#include <vector>

namespace WinAmyTests {

// A single piece placement for BuildBoardEPD: a level/file/rank coordinate plus
// the EPD piece letter (upper-case = White, lower-case = Black).
struct SPlacement {
    uint16_t wLevel;
    uint16_t wFile;
    uint16_t wRank;
    char chPiece;
};

// Build a full 15-level 3D EPD from an explicit list of piece placements. This
// lets a test position any piece on any level/file/rank — which the flat
// main-board helper (CreatePositionFromLegacyMainEPD) cannot do — so cross-level
// pawn captures can be set up deterministically.
static std::string BuildBoardEPD(const std::vector<SPlacement> &Placements,
                                 char chSideToMove) {
    char rgchPiece[CBitBoard::SIZE];
    for (uint16_t wSquare = 0; wSquare < CBitBoard::SIZE; ++wSquare) {
        rgchPiece[wSquare] = 0;
    }
    for (const SPlacement &Placement : Placements) {
        const CSCoord Coord(Placement.wLevel, Placement.wFile, Placement.wRank);
        rgchPiece[Coord.BitOffset()] = Placement.chPiece;
    }

    std::string Epd;
    for (uint16_t wLevel = 0; wLevel < CBitBoard::NUM_LEVELS; ++wLevel) {
        if (wLevel != 0) {
            Epd += '|';
        }
        const int nWidth = static_cast<int>(CBitBoard::LEVEL_WIDTH[wLevel]);
        for (int nRank = nWidth - 1; nRank >= 0; --nRank) {
            if (nRank != nWidth - 1) {
                Epd += '/';
            }
            int nEmpty = 0;
            for (int nFile = 0; nFile < nWidth; ++nFile) {
                const CSCoord Coord(wLevel, static_cast<uint16_t>(nFile),
                                    static_cast<uint16_t>(nRank));
                const char chPiece = rgchPiece[Coord.BitOffset()];
                if (chPiece == 0) {
                    ++nEmpty;
                } else {
                    if (nEmpty != 0) {
                        Epd += static_cast<char>('0' + nEmpty);
                        nEmpty = 0;
                    }
                    Epd += chPiece;
                }
            }
            if (nEmpty != 0) {
                Epd += static_cast<char>('0' + nEmpty);
            }
        }
    }
    Epd += ' ';
    Epd += chSideToMove;
    Epd += " - -";
    return Epd;
}

TEST_CLASS(SearchDataTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    // For a pawn move, promotion must be decided by the destination square:
    // a pawn landing on a promotion square must promote, and a pawn landing on
    // any other square must not.  This relationship must hold for every move
    // produced by the engine, regardless of which generator created it.
    static bool PawnPromotionInvariantHolds(CPosition *pPosition, CMove Move) {
        const uint16_t wFrom = Move.GetFromCoord().BitOffset();
        if (TYPE(pPosition->GetPiece(wFrom)) != Pawn) {
            return true;
        }
        const bool fPromoSquare = is_promo_square(Move.GetToCoord());
        return fPromoSquare == Move.HasPromotion();
    }

    // Collect the strictly-legal moves for the side to move.
    static std::vector<CMove> CollectLegalMoves(CPosition *pPosition) {
        heap_t hLegal = allocate_heap();
        push_section(hLegal);
        pPosition->LegalMoves(hLegal);
        std::vector<CMove> Moves;
        for (unsigned uIndex = hLegal->current_section->start;
             uIndex < hLegal->current_section->end; ++uIndex) {
            Moves.push_back(hLegal->data[uIndex]);
        }
        free_heap(hLegal);
        return Moves;
    }

    // Collect every move the quiescence generator produces for the side to move.
    static std::vector<CMove> CollectQuiescenceMoves(CPosition *pPosition) {
        std::unique_ptr<CSearchData> SearchData(new CSearchData(pPosition));
        SearchData->EnterNode();
        std::vector<CMove> Moves;
        CMove QMove;
        int nGuard = 0;
        while ((QMove = SearchData->NextMoveQ(-1000000)) != M_NONE &&
               nGuard++ < 4096) {
            Moves.push_back(QMove);
        }
        SearchData->LeaveNode();
        return Moves;
    }

    // True iff "Moves" contains a move with the given from/to that is a
    // promoting capture (HasPromotion && IsCapture).
    static bool ContainsPromotingCapture(const std::vector<CMove> &Moves,
                                         uint16_t wFrom, uint16_t wTo) {
        for (CMove Move : Moves) {
            if (Move.GetFromCoord().BitOffset() == wFrom &&
                Move.GetToCoord().BitOffset() == wTo && Move.IsCapture() &&
                Move.HasPromotion()) {
                return true;
            }
        }
        return false;
    }

    // True iff "Moves" contains a move with the given from/to that is a plain
    // (non-promoting) capture.
    static bool ContainsPlainCapture(const std::vector<CMove> &Moves,
                                     uint16_t wFrom, uint16_t wTo) {
        for (CMove Move : Moves) {
            if (Move.GetFromCoord().BitOffset() == wFrom &&
                Move.GetToCoord().BitOffset() == wTo && Move.IsCapture() &&
                !Move.HasPromotion()) {
                return true;
            }
        }
        return false;
    }

    TEST_METHOD(ConstructorInitializesSearchState) {
        PositionGuard position(CPosition::Initial());

        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));

        Assert::IsTrue(searchData->m_pPosition == position.get());
        Assert::IsTrue(searchData->m_pStatusTable != nullptr);
        Assert::IsTrue(searchData->m_pCurrent == searchData->m_pStatusTable);
        Assert::IsTrue(searchData->m_pKillerTable != nullptr);
        Assert::IsTrue(searchData->m_pKiller == searchData->m_pKillerTable);
        Assert::IsTrue(searchData->m_hHeap != nullptr);
        Assert::AreEqual(0, (int)searchData->m_wPly);
    }

    TEST_METHOD(EnterNodeAndLeaveNodeUpdatePlyAndPointers) {
        PositionGuard position(CPosition::Initial());
        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));

        SSearchStatus *initialStatus = searchData->m_pCurrent;
        SKillerEntry *initialKiller = searchData->m_pKiller;

        searchData->EnterNode();
        Assert::AreEqual(1, (int)searchData->m_wPly);
        Assert::IsTrue(searchData->m_pCurrent == initialStatus + 1);
        Assert::IsTrue(searchData->m_pKiller == initialKiller + 1);

        searchData->LeaveNode();
        Assert::AreEqual(0, (int)searchData->m_wPly);
        Assert::IsTrue(searchData->m_pCurrent == initialStatus);
        Assert::IsTrue(searchData->m_pKiller == initialKiller);
    }

    TEST_METHOD(NextMoveReturnsLegalMove) {
        PositionGuard position(CPosition::Initial());
        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));
        CMove expected = MakeMainBoardMove(he2, he4, M_PAWND);

        searchData->EnterNode();
        searchData->m_pCurrent->st_hashmove = expected;
        CMove move = searchData->NextMove();
        searchData->LeaveNode();

        Assert::IsTrue(move == expected);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(NextEvasionReturnsLegalMoveWhenInCheck) {
        char epd[] = "4r3/8/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));
        CMove expected = MakeMainBoardMove(he1, hd1, 0);

        searchData->EnterNode();
        searchData->m_pCurrent->st_hashmove = expected;
        CMove move = searchData->NextEvasion();
        searchData->LeaveNode();

        Assert::IsTrue(move == expected);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(NextMoveQReturnsCaptureWhenCaptureExists) {
        char epd[] = "4k3/8/8/8/4p3/8/4Q3/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));

        searchData->EnterNode();
        CMove move = searchData->NextMoveQ(-500000);
        searchData->LeaveNode();

        Assert::IsTrue(move != M_NONE);
        Assert::IsTrue(move.IsCapture());
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    // Regression for the 3D pawn-promotion bug: the quiescence capture
    // generator used to decide promotion from the pawn's source rank
    // (PrePromoRank), an 8x8-board assumption.  In 3D a pawn can capture across
    // levels onto a promotion square from a square that is not on the
    // pre-promotion rank (e.g. capturing onto bit offset 97 = level g/file
    // g/rank 0), and a pre-promotion pawn can capture across levels onto a
    // non-promotion square.  The generator must therefore decide promotion from
    // the destination square.  Play random legal games and, at every position,
    // verify every quiescence move is pseudo-legal and respects the
    // pawn-promotion invariant.
    TEST_METHOD(QuiescenceMovesRespectPromotionInvariant) {
        for (int nGame = 0; nGame < 25; nGame++) {
            InitRandom(static_cast<ran_t>(0xC0FFEEu) +
                       static_cast<ran_t>(nGame) * 2654435761u);
            PositionGuard Pos(CPosition::Initial());
            for (int nPly = 0; nPly < 80; nPly++) {
                // Collect the strictly-legal moves to pick a random one.
                heap_t hLegal = allocate_heap();
                push_section(hLegal);
                Pos.get()->LegalMoves(hLegal);
                std::vector<CMove> Moves;
                for (unsigned uIndex = hLegal->current_section->start;
                     uIndex < hLegal->current_section->end; ++uIndex) {
                    Moves.push_back(hLegal->data[uIndex]);
                }
                free_heap(hLegal);
                if (Moves.empty()) {
                    break;
                }

                // Every legal move must respect the promotion invariant.
                for (CMove Move : Moves) {
                    if (!PawnPromotionInvariantHolds(Pos.get(), Move)) {
                        std::wstringstream Message;
                        Message
                            << L"legal move violates promotion invariant: game "
                            << nGame << L" ply " << nPly << L" from "
                            << Move.GetFromCoord().BitOffset() << L" to "
                            << Move.GetToCoord().BitOffset() << L" promo "
                            << (int)Move.HasPromotion();
                        Assert::Fail(Message.str().c_str());
                    }
                }

                // Every quiescence move must be pseudo-legal and respect the
                // promotion invariant.
                std::unique_ptr<CSearchData> SearchData(
                    new CSearchData(Pos.get()));
                SearchData->EnterNode();
                CMove QMove;
                int nGuard = 0;
                while ((QMove = SearchData->NextMoveQ(-1000000)) != M_NONE &&
                       nGuard++ < 4096) {
                    if (!Pos.get()->LegalMove(QMove)) {
                        std::wstringstream Message;
                        Message << L"quiescence move is not legal: game " << nGame
                                << L" ply " << nPly << L" from "
                                << QMove.GetFromCoord().BitOffset() << L" to "
                                << QMove.GetToCoord().BitOffset() << L" promo "
                                << (int)QMove.HasPromotion() << L" cap "
                                << (int)QMove.IsCapture();
                        SearchData->LeaveNode();
                        Assert::Fail(Message.str().c_str());
                    }
                    if (!PawnPromotionInvariantHolds(Pos.get(), QMove)) {
                        std::wstringstream Message;
                        Message << L"quiescence move violates promotion "
                                   L"invariant: game "
                                << nGame << L" ply " << nPly << L" from "
                                << QMove.GetFromCoord().BitOffset() << L" to "
                                << QMove.GetToCoord().BitOffset() << L" promo "
                                << (int)QMove.HasPromotion();
                        SearchData->LeaveNode();
                        Assert::Fail(Message.str().c_str());
                    }
                }
                SearchData->LeaveNode();

                Pos.get()->DoMove(Moves[Random64() % Moves.size()]);
            }
        }
    }

    // Targeted edge-case coverage for the 3D quiescence capture generator
    // (EmitQCapture).  These EPDs are engine-produced (round-tripping) and each
    // contains at least one PAWN cross-level CAPTURE whose destination is a
    // promotion square.  Regardless of the pawn's source rank/level, such a
    // capture must promote, and the quiescence generator must emit the promoting
    // capture exactly like the authoritative LegalMoves generator.  Applying the
    // move must leave a (queen) promotion piece on the destination.
    TEST_METHOD(QuiescenceGeneratesCrossLevelPromotionCaptures) {
        static const char *const rgpszEpd[] = {
            // White pawn (offset 150) captures cross-level onto promotion
            // square offset 56.
            "1|2/2|3/3/3|4/4/4/4|5/1n3/5/5/5|6/6/Q5/6/6/1q4|ppppppp/7/7/7/7/"
            "3P2P/PPP1PP1|r2qkbnr/ppp2ppp/2n1p3/8/3p4/7b/PPPPPPPP/RNB1KBNR|"
            "r1b2br/ppppppp/n6/7/7/PPPPPPP/RNBQNBR|pppppp/6/6/6/4P1/PPPP1P|"
            "5/5/5/5/5|4/4/4/4|3/3/3|2/2|1 w KQkq -",
            // Black pawn (offset 189) captures cross-level onto promotion
            // square offset 85.
            "1|2/2|3/3/3|4/4/4/4|5/1n3/5/5/5|Q5/6/6/6/6/6|ppppppp/7/7/7/7/"
            "3PN1P/PPP1PP1|r2qkbnr/ppp2ppp/2n1p3/8/3p4/7b/PPPPPPPP/RNB1KB1R|"
            "r1b2br/ppppppp/n6/7/7/PPPPPPP/RNBQNBR|pppppp/6/6/6/4P1/PPPP1P|"
            "5/5/3q1/5/5|4/4/4/4|3/3/3|2/2|1 b KQkq -",
        };

        for (const char *pszEpd : rgpszEpd) {
            PositionGuard Pos(CPosition::CreateFromEPD(pszEpd));
            Assert::IsTrue(Pos.get() != nullptr, L"EPD must parse");

            const std::vector<CMove> LegalMoves = CollectLegalMoves(Pos.get());
            const std::vector<CMove> QMoves = CollectQuiescenceMoves(Pos.get());

            int nCrossLevelPromoCaptures = 0;
            for (CMove Move : LegalMoves) {
                const uint16_t wFrom = Move.GetFromCoord().BitOffset();
                const uint16_t wTo = Move.GetToCoord().BitOffset();
                if (TYPE(Pos.get()->GetPiece(wFrom)) != Pawn ||
                    Move.GetFromCoord().m_nLevel == Move.GetToCoord().m_nLevel ||
                    !is_promo_square(Move.GetToCoord()) || !Move.IsCapture() ||
                    !Move.HasPromotion()) {
                    continue;
                }
                nCrossLevelPromoCaptures++;

                // The destination is a promotion square, so the move must
                // satisfy the promotion invariant.
                Assert::IsTrue(PawnPromotionInvariantHolds(Pos.get(), Move));

                // The quiescence generator must emit the same promoting
                // capture (decided by the destination, not the source rank).
                std::wstringstream Message;
                Message << L"quiescence generator must emit cross-level "
                           L"promotion capture from "
                        << wFrom << L" to " << wTo;
                Assert::IsTrue(ContainsPromotingCapture(QMoves, wFrom, wTo),
                               Message.str().c_str());

                // The move must be legal and, when played, must leave the
                // promoted piece (of the move's own promotion type) on the
                // destination square.
                Assert::IsTrue(Pos.get()->LegalMove(Move));
                PositionGuard Child(CPosition::Clone(Pos.get()));
                Child.get()->DoMove(Move);
                Assert::AreEqual((int)Move.GetPromotionType(),
                                 (int)TYPE(Child.get()->GetPiece(wTo)),
                                 L"cross-level promotion capture must leave the "
                                 L"promoted piece on the destination");
            }

            Assert::IsTrue(nCrossLevelPromoCaptures > 0,
                           L"EPD must expose a cross-level promotion capture");
        }
    }

    // Inverse edge case: a pawn sitting on the pre-promotion rank that captures
    // ACROSS LEVELS onto a NON-promotion square must NOT promote.  The old
    // generator keyed promotion off the source rank (PrePromoRank), so it would
    // wrongly promote here; the destination-driven generator must emit a plain
    // capture, matching the authoritative LegalMoves generator.
    TEST_METHOD(QuiescencePrePromotionPawnCrossLevelNonPromoCaptureDoesNotPromote) {
        // White pawn (offset 277, on the pre-promotion rank) captures
        // cross-level onto NON-promotion square offset 181.
        const char *pszEpd =
            "1|1q/r1|3/3/3|4/4/4/4|5/3q1/5/5/5|4n1/6/6/5N/6/6|p2p3/2p2pp/4p2/"
            "1p5/3P3/6P/1PP1PP1|rnb2b2/p2pppp1/1pp5/1N4np/1P3P2/3P4/P1PBP1PP/"
            "1Q2QB1R|r1bk1br/ppppppp/2n1B2/7/P1PPPP1/1P1KN1P/5BR|pp3p/P1p3/4p1/"
            "6/1P3P/2PPP1|5/5/5/5/5|4/4/4/1R2|3/3/3|2/2|1 w - -";

        PositionGuard Pos(CPosition::CreateFromEPD(pszEpd));
        Assert::IsTrue(Pos.get() != nullptr, L"EPD must parse");

        const std::vector<CMove> LegalMoves = CollectLegalMoves(Pos.get());
        const std::vector<CMove> QMoves = CollectQuiescenceMoves(Pos.get());

        int nCrossLevelNonPromoCaptures = 0;
        for (CMove Move : LegalMoves) {
            const uint16_t wFrom = Move.GetFromCoord().BitOffset();
            const uint16_t wTo = Move.GetToCoord().BitOffset();
            if (TYPE(Pos.get()->GetPiece(wFrom)) != Pawn ||
                Move.GetFromCoord().m_nLevel == Move.GetToCoord().m_nLevel ||
                is_promo_square(Move.GetToCoord()) || !Move.IsCapture() ||
                !PrePromoRank[Pos.get()->GetTurn()].TstBit(wFrom)) {
                continue;
            }
            nCrossLevelNonPromoCaptures++;

            // A non-promotion destination must NOT carry a promotion.
            Assert::IsFalse(Move.HasPromotion());
            Assert::IsTrue(PawnPromotionInvariantHolds(Pos.get(), Move));

            // The quiescence generator must emit the same plain capture.
            std::wstringstream Message;
            Message << L"quiescence generator must emit plain cross-level "
                       L"capture from "
                    << wFrom << L" to " << wTo;
            Assert::IsTrue(ContainsPlainCapture(QMoves, wFrom, wTo),
                           Message.str().c_str());
            Assert::IsTrue(Pos.get()->LegalMove(Move));
        }

        Assert::IsTrue(
            nCrossLevelNonPromoCaptures > 0,
            L"EPD must expose a pre-promotion-rank cross-level non-promotion "
            L"capture");
    }

    // Every move the quiescence generator emits must also be a legal capture,
    // and no move it emits may violate the promotion invariant.
    TEST_METHOD(QuiescenceMovesAreLegalCapturesRespectingInvariant) {
        static const char *const rgpszEpd[] = {
            "1|2/2|3/3/3|4/4/4/4|5/1n3/5/5/5|6/6/Q5/6/6/1q4|ppppppp/7/7/7/7/"
            "3P2P/PPP1PP1|r2qkbnr/ppp2ppp/2n1p3/8/3p4/7b/PPPPPPPP/RNB1KBNR|"
            "r1b2br/ppppppp/n6/7/7/PPPPPPP/RNBQNBR|pppppp/6/6/6/4P1/PPPP1P|"
            "5/5/5/5/5|4/4/4/4|3/3/3|2/2|1 w KQkq -",
            "1|1q/r1|3/3/3|4/4/4/4|5/3q1/5/5/5|4n1/6/6/5N/6/6|p2p3/2p2pp/4p2/"
            "1p5/3P3/6P/1PP1PP1|rnb2b2/p2pppp1/1pp5/1N4np/1P3P2/3P4/P1PBP1PP/"
            "1Q2QB1R|r1bk1br/ppppppp/2n1B2/7/P1PPPP1/1P1KN1P/5BR|pp3p/P1p3/4p1/"
            "6/1P3P/2PPP1|5/5/5/5/5|4/4/4/1R2|3/3/3|2/2|1 w - -",
        };

        for (const char *pszEpd : rgpszEpd) {
            PositionGuard Pos(CPosition::CreateFromEPD(pszEpd));
            Assert::IsTrue(Pos.get() != nullptr, L"EPD must parse");

            const std::vector<CMove> QMoves = CollectQuiescenceMoves(Pos.get());
            for (CMove QMove : QMoves) {
                Assert::IsTrue(QMove.IsCapture(),
                               L"quiescence move must be a capture");
                Assert::IsTrue(Pos.get()->LegalMove(QMove),
                               L"quiescence move must be legal");
                Assert::IsTrue(PawnPromotionInvariantHolds(Pos.get(), QMove),
                               L"quiescence move must respect the promotion "
                               L"invariant");
            }
        }
    }

    TEST_METHOD(PutKillerTracksAndPromotesByHitCount) {
        PositionGuard position(CPosition::Initial());
        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));

        CMove moveOne = MakeMainBoardMove(he2, he4, M_PAWND);
        CMove moveTwo = MakeMainBoardMove(hd2, hd4, M_PAWND);

        searchData->PutKiller(moveOne);
        Assert::IsTrue(searchData->m_pKiller->killer1 == moveOne);
        Assert::AreEqual((uint32_t)1, searchData->m_pKiller->kcount1);

        searchData->PutKiller(moveTwo);
        Assert::IsTrue(searchData->m_pKiller->killer2 == moveTwo);
        Assert::AreEqual((uint32_t)1, searchData->m_pKiller->kcount2);

        searchData->PutKiller(moveTwo);
        Assert::IsTrue(searchData->m_pKiller->killer1 == moveTwo);
        Assert::IsTrue(searchData->m_pKiller->killer2 == moveOne);
        Assert::AreEqual((uint32_t)2, searchData->m_pKiller->kcount1);
        Assert::AreEqual((uint32_t)1, searchData->m_pKiller->kcount2);
    }
};

} // namespace WinAmyTests
