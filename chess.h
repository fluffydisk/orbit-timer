/* Local two-player chess. Positive pieces are white, negative are black. */
enum { PAWN=1, KNIGHT, BISHOP, ROOK, QUEEN, KING };
typedef struct {
    int board[64], side, castle, ep, selected, from, to, halfmoves;
} Chess;

static void chess_reset(Chess *g) {
    *g = (Chess){.side=1, .castle=15, .ep=-1, .selected=-1, .from=-1, .to=-1};
    int back[] = {ROOK,KNIGHT,BISHOP,QUEEN,KING,BISHOP,KNIGHT,ROOK};
    for (int i=0; i<8; ++i) {
        g->board[i]=-back[i];
        g->board[8+i]=-PAWN;
        g->board[48+i]=PAWN;
        g->board[56+i]=back[i];
    }
}

static bool geometry(const Chess *g, int a, int b, bool attack) {
    int piece=g->board[a], side=piece>0?1:-1;
    int x=b%8-a%8, y=b/8-a/8, dx=abs(x), dy=abs(y);
    switch (abs(piece)) {
    case PAWN:
        if (dx==1 && y==-side)
            return attack || g->board[b]*side<0 || b==g->ep;
        if (attack || x || g->board[b]) return false;
        return y==-side || (y==-2*side && a/8==(side>0?6:1) && !g->board[a-8*side]);
    case KNIGHT: return dx*dy==2;
    case KING: return dx<=1 && dy<=1;
    case BISHOP: if (dx!=dy) return false; break;
    case ROOK: if (dx && dy) return false; break;
    case QUEEN: if (dx && dy && dx!=dy) return false; break;
    default: return false;
    }
    int step=(x>0)-(x<0)+8*((y>0)-(y<0));
    for (int i=a+step; i!=b; i+=step)
        if (g->board[i]) return false;
    return true;
}

static bool attacked(const Chess *g, int square, int side) {
    for (int i=0; i<64; ++i)
        if (i!=square && g->board[i]*side>0 && geometry(g,i,square,true)) return true;
    return false;
}
static bool checked(const Chess *g, int side) {
    for (int i=0; i<64; ++i)
        if (g->board[i]==side*KING) return attacked(g,i,-side);
    return true;
}
static bool chess_move(Chess *g, int a, int b, int promotion) {
    if (a<0 || b<0 || a>=64 || b>=64 || a==b) return false;
    int side=g->side, piece=g->board[a]*side, target=g->board[b];
    if (piece<=0 || target*side>0 || abs(target)==KING) return false;
    Chess next=*g;
    bool castle=piece==KING && a/8==b/8 && abs(a-b)==2;
    if (castle) {
        int step=b>a?1:-1, rook=a+(step>0?3:-4);
        int right=(side>0?1:4)*(step>0?1:2);
        if (a!=(side>0?60:4) || !(g->castle&right) || g->board[rook]!=side*ROOK ||
            checked(g,side)) return false;
        for (int i=a+step; i!=rook; i+=step)
            if (g->board[i]) return false;
        Chess middle=*g;
        middle.board[a]=0;
        middle.board[a+step]=side*KING;
        if (checked(&middle,side)) return false;
        next.board[rook]=0;
        next.board[a+step]=side*ROOK;
    } else if (!geometry(g,a,b,false)) return false;

    next.board[a]=0;
    next.board[b]=side*piece;
    next.ep=-1;
    if (piece==PAWN) {
        if (b==g->ep && !target && a%8!=b%8) next.board[b+8*side]=0;
        if (abs(a-b)==16) next.ep=(a+b)/2;
        if (b/8==0 || b/8==7) next.board[b]=side*promotion;
    }
    if (piece==KING) next.castle&=side>0?12:3;
    const int corners[]={63,56,7,0};
    for (int i=0; i<4; ++i)
        if (a==corners[i] || b==corners[i]) next.castle&=~(1<<i);
    if (checked(&next,side)) return false;
    next.side=-side;
    next.selected=-1;
    next.from=a;
    next.to=b;
    next.halfmoves=piece==PAWN || target?0:g->halfmoves+1;
    *g=next;
    return true;
}
static bool chess_has_move(const Chess *g) {
    for (int a=0; a<64; ++a) {
        if (g->board[a]*g->side<=0) continue;
        for (int b=0; b<64; ++b) {
            Chess copy=*g;
            if (chess_move(&copy,a,b,QUEEN)) return true;
        }
    }
    return false;
}

static bool chess_test(void) {
    Chess g;
    chess_reset(&g);
    if (chess_move(&g,52,28,QUEEN) || !chess_move(&g,53,45,QUEEN) ||
        !chess_move(&g,12,28,QUEEN) || !chess_move(&g,54,38,QUEEN) ||
        !chess_move(&g,3,39,QUEEN)) return false;
    if (!checked(&g,1) || chess_has_move(&g)) return false;
    chess_reset(&g);
    return chess_move(&g,52,36,QUEEN) && chess_move(&g,8,16,QUEEN) &&
           chess_move(&g,36,28,QUEEN) && chess_move(&g,11,27,QUEEN) &&
           chess_move(&g,28,19,QUEEN) && !g.board[27];
}
