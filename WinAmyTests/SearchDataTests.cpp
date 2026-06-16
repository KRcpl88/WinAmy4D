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

    // Collect every legal move generated by the full legal move generator.
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

    // Collect every move produced by the quiescence generator (captures and
    // promotion pushes) for the side to move at the given position.
    static std::vector<CMove> CollectQuiescenceMoves(CPosition *pPosition) {
        std::unique_ptr<CSearchData> SearchData(new CSearchData(pPosition));
        SearchData->EnterNode();
        std::vector<CMove> Moves;
        CMove Move;
        int nGuard = 0;
        while ((Move = SearchData->NextMoveQ(-1000000)) != M_NONE &&
               nGuard++ < 4096) {
            Moves.push_back(Move);
        }
        SearchData->LeaveNode();
        return Moves;
    }

    // Edge case: a white pawn that is NOT on a pre-promotion rank captures
    // across levels onto a promotion square.  The pawn sits on the h-level (7)
    // file 1 rank 1 and captures the black knight on the j-level (9) promotion
    // square file 0 rank 0.  Promotion must be decided from the destination, so
    // the capture must promote even though the source rank is the white home
    // rank.  Verified through the FULL legal move generator: it must emit one
    // legal promotion capture for EACH piece the pawn may become, and DoMove
    // must replace the pawn with the promoted piece on the target square.
    TEST_METHOD(LegalMoveGeneratorPromotesCrossLevelCaptureToEachPiece) {
        const uint16_t wFrom = CSCoord(7, 1, 1).BitOffset();
        const uint16_t wTo = CSCoord(9, 0, 0).BitOffset();
        const std::string Epd = BuildBoardEPD(
            {{7, 4, 0, 'K'}, {7, 4, 7, 'k'}, {7, 1, 1, 'P'}, {9, 0, 0, 'n'}}, 'w');
        PositionGuard Pos(CPosition::CreateFromEPD(Epd.c_str()));
        Assert::IsTrue(Pos.get() != nullptr);
        Assert::IsFalse(Pos.get()->InCheck(White));
        Assert::IsFalse(Pos.get()->InCheck(Black));
        Assert::IsTrue(is_promo_square(CSCoord(wTo)));
        Assert::AreEqual((int)Pawn, (int)Pos.get()->GetPiece(wFrom));
        Assert::AreEqual((int)-Knight, (int)Pos.get()->GetPiece(wTo));

        std::vector<CMove> Moves = CollectLegalMoves(Pos.get());

        bool rgfSeenPromoType[King + 1] = {false};
        int nPromoCaptures = 0;
        for (CMove Move : Moves) {
            if (Move.GetFromCoord().BitOffset() != wFrom ||
                Move.GetToCoord().BitOffset() != wTo) {
                continue;
            }
            Assert::IsTrue(Move.IsCapture());
            Assert::IsTrue(Move.HasPromotion());
            Assert::IsTrue(is_promo_square(Move.GetToCoord()));
            const int nType = Move.GetPromotionType();
            Assert::IsTrue(nType >= Knight && nType <= Queen);
            rgfSeenPromoType[nType] = true;
            nPromoCaptures++;

            // DoMove must promote: the target square holds the (white) promoted
            // piece and the source square is vacated.
            PositionGuard After(CPosition::Clone(Pos.get()));
            After.get()->DoMove(Move);
            Assert::AreEqual((int)nType, (int)After.get()->GetPiece(wTo));
            Assert::AreEqual((int)Neutral, (int)After.get()->GetPiece(wFrom));
        }

        Assert::AreEqual(4, nPromoCaptures);
        Assert::IsTrue(rgfSeenPromoType[Queen]);
        Assert::IsTrue(rgfSeenPromoType[Rook]);
        Assert::IsTrue(rgfSeenPromoType[Bishop]);
        Assert::IsTrue(rgfSeenPromoType[Knight]);
    }

    // The quiescence generator must also promote a cross-level capture that
    // lands on a promotion square (it emits the queen promotion as the dominant
    // capture).  Same position as the legal-move test above.
    TEST_METHOD(QuiescenceGeneratorPromotesCrossLevelCapture) {
        const uint16_t wFrom = CSCoord(7, 1, 1).BitOffset();
        const uint16_t wTo = CSCoord(9, 0, 0).BitOffset();
        const std::string Epd = BuildBoardEPD(
            {{7, 4, 0, 'K'}, {7, 4, 7, 'k'}, {7, 1, 1, 'P'}, {9, 0, 0, 'n'}}, 'w');
        PositionGuard Pos(CPosition::CreateFromEPD(Epd.c_str()));
        Assert::IsTrue(Pos.get() != nullptr);
        Assert::IsFalse(Pos.get()->InCheck(White));

        std::vector<CMove> Moves = CollectQuiescenceMoves(Pos.get());

        bool fSeenPromotionCapture = false;
        for (CMove Move : Moves) {
            // Every quiescence move must respect the destination-driven
            // promotion invariant and be legal.
            Assert::IsTrue(Pos.get()->LegalMove(Move));
            Assert::IsTrue(PawnPromotionInvariantHolds(Pos.get(), Move));
            if (Move.GetFromCoord().BitOffset() == wFrom &&
                Move.GetToCoord().BitOffset() == wTo) {
                Assert::IsTrue(Move.IsCapture());
                Assert::IsTrue(Move.HasPromotion());
                fSeenPromotionCapture = true;
            }
        }
        Assert::IsTrue(fSeenPromotionCapture);
    }

    // Converse edge case (the other half of the original bug): a white pawn ON
    // a pre-promotion rank captures across levels onto a NON-promotion square,
    // which must NOT promote, while its other captures/pushes that land on
    // promotion squares MUST promote.  The pawn on the g-level (6) file 0 rank 5
    // (a pre-promotion rank) has three relevant moves:
    //   * capture onto the g-level (6) file 1 rank 6 promotion square  -> promote
    //   * capture across to the i-level (8) file 0 rank 5 (non-promo)   -> NO promote
    //   * non-capturing push onto the g-level (6) file 0 rank 6 promo   -> promote
    TEST_METHOD(LegalMoveGeneratorPromotionIsDestinationDriven) {
        const uint16_t wFrom = CSCoord(6, 0, 5).BitOffset();
        const uint16_t wPromoCapture = CSCoord(6, 1, 6).BitOffset();
        const uint16_t wNonPromoCapture = CSCoord(8, 0, 5).BitOffset();
        const uint16_t wPromoPush = CSCoord(6, 0, 6).BitOffset();
        const std::string Epd = BuildBoardEPD({{7, 4, 0, 'K'},
                                               {7, 4, 7, 'k'},
                                               {6, 0, 5, 'P'},
                                               {6, 1, 6, 'n'},
                                               {8, 0, 5, 'n'}},
                                              'w');
        PositionGuard Pos(CPosition::CreateFromEPD(Epd.c_str()));
        Assert::IsTrue(Pos.get() != nullptr);
        Assert::IsFalse(Pos.get()->InCheck(White));
        Assert::IsFalse(Pos.get()->InCheck(Black));
        Assert::IsTrue(is_promo_square(CSCoord(wPromoCapture)));
        Assert::IsFalse(is_promo_square(CSCoord(wNonPromoCapture)));
        Assert::IsTrue(is_promo_square(CSCoord(wPromoPush)));

        std::vector<CMove> Moves = CollectLegalMoves(Pos.get());

        int nPromoCaptures = 0;
        int nNonPromoCaptures = 0;
        int nPromoPushes = 0;
        for (CMove Move : Moves) {
            if (Move.GetFromCoord().BitOffset() != wFrom) {
                continue;
            }
            Assert::IsTrue(PawnPromotionInvariantHolds(Pos.get(), Move));
            const uint16_t wDest = Move.GetToCoord().BitOffset();
            if (wDest == wPromoCapture) {
                Assert::IsTrue(Move.IsCapture());
                Assert::IsTrue(Move.HasPromotion());
                nPromoCaptures++;
            } else if (wDest == wNonPromoCapture) {
                Assert::IsTrue(Move.IsCapture());
                Assert::IsFalse(Move.HasPromotion());
                nNonPromoCaptures++;
            } else if (wDest == wPromoPush) {
                Assert::IsFalse(Move.IsCapture());
                Assert::IsTrue(Move.HasPromotion());
                nPromoPushes++;
            }
        }

        // One legal move per promotion piece on the two promotion squares, and
        // exactly one plain (non-promoting) capture on the non-promotion square.
        Assert::AreEqual(4, nPromoCaptures);
        Assert::AreEqual(1, nNonPromoCaptures);
        Assert::AreEqual(4, nPromoPushes);
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
