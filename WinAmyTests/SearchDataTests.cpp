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
