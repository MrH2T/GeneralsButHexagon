#include<iostream>
#include<cmath>
#include<cstdio>
#include<ctime>
#include<string>
#include<sstream>
#include<set>
#include<map>
#include<algorithm>
#include<deque>
#include<vector>
#include<locale>
#include<codecvt>

#include<winsock2.h>
#include<windows.h>
#include<gdiplus.h>

#include"asio.hpp"
using namespace Gdiplus;

#define GBHVersion "dev0.01"

const wchar_t wndClassName[] = L"GBH Winclass";

int max(int a,int b){return a>b?a:b;}

const Color bgColor = 0xFF222222;
const Color ukColor = 0xFF404040;
const Color themeColor = 0xFFD07000;
const Color unclLandColor = 0xFFDDDDDD;
const Color unclCityColor = 0xFF808080;
const Color swampColor = 0xFF808080;
const Color mountColor = 0xFFBBBBBB;
FontFamily ffSs(L"等线");
const Color PlayerColor[13] = {
    0xFFDDDDDD,
	Color::Red,
	Color::Blue,
	Color::Green,
	Color::Orange,
	Color::DeepPink,
	Color::Brown,
	Color::Purple,
	Color::LightBlue,
	Color::SkyBlue,
	Color::DarkGreen,
	Color::SeaGreen,
	Color::Gold
};
const Color &ntcolor = PlayerColor[0]; //neutralTroop

const wchar_t title[] = L"Generals But Hexagon",
            titleAbbr[] = L"GBH";
const int maxTeam = 12, maxPlayer = 20;

#define VK_(x) (#x[0]-'A'+0x41)


MSG msg;
HWND hwnd;
WNDCLASSEX wc;
PAINTSTRUCT ps;

namespace Gdip{
    ULONG_PTR m_gdiplusToken;
    GdiplusStartupInput StartupInput;
    Image *imgGeneral = nullptr;
    Image *imgCity = nullptr;
    Image *imgMount = nullptr;
    Image *imgObstacle = nullptr;
    Image *imgSwamp = nullptr;
}
namespace Network{
	asio::io_context ioc;
	std::string sIP;
	asio::ip::tcp::socket soc(ioc);
	std::array<char,65536> buf;
	asio::error_code err;
	bool connServer(){
		try{
			asio::connect(
					soc,
					asio::ip::tcp::resolver(ioc).resolve(sIP, std::to_string(11451)));
			return true;
		}catch(std::exception &e){return false;}
		return false;
	}
	std::string recvMsg(int len){
		asio::read(soc,asio::buffer(buf,len))
		// size_t len=soc.read_some(asio::buffer(buf),err);
		std::string res("");
		res=buf.data();
		res.resize(len);
		return res;
	}
	bool sendMsg(std::string msg){
		try
        {
            asio::write(soc, asio::buffer(msg));
			return true;
		}
        catch (std::exception &e)
        {
			return false;
        }
		return false;
	}
	bool closeSocket(){
		try{
			soc.close();
			return true;
		}catch(std::exception &e){return false;}
		return false;
	}
}

namespace Game{

    int page,tab[16];
	bool scp;
	#define IN_MENU 0
	#define IN_LOBBY 1
	#define IN_GAME 3
    int turn;
    clock_t mousedownTime;
    bool mousedown;
	void setPage(int x){page=x;}
    struct Player{
        std::wstring name;
        int team,id;
        int army,land;
        Player(int id): name(),team(),id(id){}
        Player(const std::wstring &name=L"Anonymous", int team=0, int id=-1): name(name),team(team),id(id){}

    }me,player[maxPlayer+1];
    std::set<int> team_member[1+maxTeam];   //0: spec

    struct TeamInfo{
		#define UNKNOWN -1
		#define GENERAL 1
		#define CITY 2
		#define MOUNT 3
		#define SWAMP 4
		#define OBSTACLE 5
        int id, army, land;
        TeamInfo(int id=0 ,int army=0, int land=-1):id(id),army(army),land(land){}
        bool operator<(const TeamInfo &tmp){return this->army>tmp.army||this->army==tmp.army&&this->land>tmp.land;}
    };
    std::vector<TeamInfo> rank;
    int readyPlayers;
    struct Coord{
        int x,y;
        Coord():x(0),y(0){}
        Coord(int x,int y):x(x),y(y){}
        bool operator==(Coord b){return x==b.x&&y==b.y;}
        bool operator!=(Coord b){return !(*this==b);}
		bool operator<(const Coord &b)const{return this->x<b.x||this->x==b.x&&this->y<b.y;}
    }curTile,myGeneral;
    inline Coord mU(Coord P){return Coord(P.x-1,P.y-1);}
    inline Coord mD(Coord P){return Coord(P.x+1,P.y+1);}
    inline Coord mLU(Coord P){return Coord(P.x,P.y-1);}
    inline Coord mLD(Coord P){return Coord(P.x+1,P.y);}
    inline Coord mRU(Coord P){return Coord(P.x-1,P.y);}
    inline Coord mRD(Coord P){return Coord(P.x,P.y+1);}
	inline Coord MV(Coord P, int dir){
		switch(dir){
			case 0:return mU(P);
			case 1:return mRU(P);
			case 2:return mRD(P);
			case 3:return mD(P);
			case 4:return mLD(P);
			case 5:return mLU(P);
			default:return Coord();
		}
		return Coord();
	}

	int lmx,lmy,ox,oy;
	int tileSize;

	struct Operation{
		int fx,fy,dir;
		bool half;
		Operation():fx(0),fy(0),dir(0),half(0){}
		Operation(int fx=0,int fy=0,int dir=0,bool half=0):fx(fx),fy(fy),dir(dir),half(half){}
	};
	struct Tile{
		int troop,land,owner;
	};
	class VisMap{
		private:
		#define HALF 1<<3
		std::map<Coord,Tile> tiles;
		std::deque<Operation> opQue;
		unsigned width;
		int mxX();
		int gybx(int x); //get y by x
		bool inRange(Coord P);
		public:
		void resize(int);
		void draw(Gdiplus::Graphics&);
		bool pushMove(int);
		bool undoMove();
		bool clearQue();
	}vmp;
	int VisMap::mxX() {return VisMap::width*2-1;}
	int VisMap::gybx(int x){
		return 2 * VisMap::width - 1 - (x-width<0?width-x:x-width);
	}
	bool VisMap::inRange(Coord P){
		if(P.x<1 || P.x >VisMap::mxX() || P.y<1 || P.y>VisMap::gybx(P.x))return false;
		return true;
	}
	void VisMap::resize(int x){
		VisMap::width=x;
	}
	bool VisMap::pushMove(int mv){
		Coord P=curTile;
		int dir=mv&7;
		Coord Q=MV(P,dir);
		if(!inRange(Q)||VisMap::tiles[Q].land==MOUNT)return false;
		VisMap::opQue.push_back(Operation(P.x,P.y,dir,mv&HALF));
		curTile=Q;	//point to the next one
		return true;
	}
	bool VisMap::undoMove(){
		if(!VisMap::opQue.empty()){
			opQue.pop_back();
			return true;
		}
		return false;
	}
	bool VisMap::clearQue(){
		if(!VisMap::opQue.empty()){
			VisMap::opQue.clear();
			return true;
		}
		return false;
	}
	void VisMap::draw(Gdiplus::Graphics& g){

	}
	bool changeMyTeam(int t){
		return false;
	}
	void switchReady(){
		return ;
	}
	bool startCli() {
		int res = 0;
		res=Network::connServer();
		if(!res){
			Network::closeSocket();
			MessageBox(hwnd, L"Failed to connect to the server", titleAbbr, MB_ICONERROR);
			return false;
		}
		Network::sendMsg(std::to_string(strlen(GBHVersion))+" "+GBHVersion);
		std::string le=Network::recvMsg(2);
		std::string bk=Network::recvMsg(le[0]-'0');
		if(bk=="nv"){
			Network::closeSocket();
			MessageBox(hwnd,L"Not the correct version",titleAbbr, MB_ICONERROR);
			return false;
		}
		else if(bk=="ok"){
			
			return true;
		}
		else return false;
		// strcpy(sendbuf + 2, g2dVerStr);
		// send(sockClient, sendbuf, 2 + strlen(g2dVerStr) + 1, 0);
		// res = recv(sockClient, recvbuf, 2, 0);
		// res = recv(sockClient, recvbuf + 2, *(unsigned short *)recvbuf, 0);
		// {
		// 	if(strcmp(recvbuf + 2, "!ver") == 0) {	//incorrect version
		// 		closesocket(sockClient);
		// 		MessageBox(hwnd, L"服务器与你的游戏版本不兼容。", titleAbbr, MB_ICONERROR);
		// 		return false;
		// 	} else if(strcmp(recvbuf + 2, "kick") == 0) {	//disconnect
		// 		closesocket(sockClient);
		// 		MessageBox(hwnd, L"服务器断开连接。", titleAbbr, MB_ICONERROR);
		// 		return false;
		// 	} else {
		// 		std::stringstream _ss(recvbuf + 2);
		// 		std::string iverGameName;
		// 		_ss >> iverGameName;
		// 		if(iverGameName != "fine") {	//be ok to join
		// 			if(iverGameName != g2dVerGameName) {
		// 				closesocket(sockClient);
		// 				MessageBox(hwnd, L"服务器不是 G2D 游戏服务器。", titleAbbr, MB_ICONERROR);
		// 				return false;
		// 			}
		// 			wchar_t _tmpbuf[65536] = { '0' };
		// 			MultiByteToWideChar(65001, 0, recvbuf + 2, -1, _tmpbuf, 65536);
		// 			MessageBox(hwnd, ((std::wstring)L"警告：服务器游戏版本与当前版本有差异，为 " + _tmpbuf + L"\n按【确定】以继续。").c_str(), titleAbbr, MB_ICONWARNING);
		// 		}
		// 	}
		// }
		// puts("successfully connected to server.");
		// strcpy(sendbuf + 2, "name");
		// *(unsigned short *)sendbuf = 5 + WideCharToMultiByte(65001, 0, me.pname.c_str(), -1, sendbuf + 7, 65536, NULL, NULL);
		// send(sockClient, sendbuf, 2 + *(unsigned short *)sendbuf, 0);
		// side = -1;
		// setPage(1);
		// WSAAsyncSelect(sockClient, hwnd, WM_SOCKCLIENT, FD_READ | FD_CLOSE);
		// return true;
	}
	std::wstring to_wide_string(const std::string& s){
		std::wstring_convert<std::codecvt_utf8<wchar_t> >converter;
		return converter.from_bytes(s);
	}
	void Paint(HDC hdc) {
		RECT rcCli;
		GetClientRect(hwnd, &rcCli);
		Bitmap bmp(rcCli.right, rcCli.bottom);
		Gdiplus::Graphics gBmp(&bmp), gDest(hdc);
		Gdiplus::Graphics &g = gBmp;

		g.Clear(bgColor);

		StringFormat sfCenter;
		sfCenter.SetAlignment(StringAlignment::StringAlignmentCenter);
		sfCenter.SetLineAlignment(StringAlignment::StringAlignmentCenter);
		Gdiplus::SolidBrush sbTheme(themeColor);
		Gdiplus::Font fTitle(L"等线", 36, FontStyleBold, UnitPoint, NULL);
		Gdiplus::Font fSubtitle(L"等线", 16, FontStyleRegular, UnitPoint, NULL);
		Gdiplus::SolidBrush sbWhite(Gdiplus::Color::White);
		Gdiplus::SolidBrush sbBlack(Gdiplus::Color::Black);
		switch(page) {
			case 0: { // page 0 Paint
				Gdiplus::RectF rcTitle(0, 120, rcCli.right, 100);
				g.DrawString(title, -1, &fTitle, rcTitle, &sfCenter, &sbWhite);

				// RectF rcSubtitle(0, 220, rcCli.right, 50);
				// g.DrawString(subtitle_ar[curSubtitle].c_str(), -1, &fSubtitle, rcSubtitle, &sfCenter, &sbWhite);

				Gdiplus::RectF rcPname(rcCli.right / 2 - 240, rcCli.bottom / 2, 480, 40);
				Gdiplus::SolidBrush sbLGrey(Gdiplus::Color::LightGray);
				g.FillRectangle((tab[0] ? &sbLGrey : &sbWhite), rcPname);
				g.DrawString(me.name.c_str(), -1, &fSubtitle, rcPname, &sfCenter, &sbTheme);

				Gdiplus::Font fBtn(L"等线", 16, FontStyleBold, UnitPoint, NULL);
				// Gdiplus::RectF rcBtnPlay1(rcCli.right / 2 - 120, rcCli.bottom / 2 + 60, 240, 50);
				// g.FillRectangle(&sbWhite, rcBtnPlay1);
				// g.DrawString(L"Start a game | Server", -1, &fBtn, rcBtnPlay1, &sfCenter, &sbTheme);

				Gdiplus::RectF rcBtnPlay2(rcCli.right / 2 - 120, rcCli.bottom / 2 + 120, 240, 50);
				g.FillRectangle(&sbWhite, rcBtnPlay2);
				g.DrawString(L"Join a game", -1, &fBtn, rcBtnPlay2, &sfCenter, &sbTheme);

				if(tab[0] == 2) {
					Gdiplus::SolidBrush sbCov(0x80000000);
					g.FillRectangle(&sbCov, Rect(0, 0, rcCli.right, rcCli.bottom));

					Gdiplus::RectF rcInputBox(rcCli.right / 2 - 200, rcCli.bottom / 2 - 160, 400, 320);
					g.FillRectangle(&sbWhite, rcInputBox);

					Gdiplus::RectF rcInputBoxInfo(rcCli.right / 2 - 160, rcCli.bottom / 2 - 110, 320, 70);
					g.DrawString(L"请输入服务器 IP 地址：", -1, &fSubtitle, rcInputBoxInfo, NULL, &sbBlack);

					Gdiplus::RectF rcInputBoxText(rcCli.right / 2 - 160, rcCli.bottom / 2 - 30, 320, 50);
					g.FillRectangle(&sbLGrey, rcInputBoxText);
					g.DrawString(to_wide_string(Network::sIP).c_str(), -1, &fSubtitle, rcInputBoxText, &sfCenter, &sbBlack);
					Gdiplus::RectF rcInputBoxBtn(rcCli.right / 2 - 40, rcCli.bottom / 2 + 60, 80, 40);
					g.FillRectangle(&sbTheme, rcInputBoxBtn);
					g.DrawString(L"OK", -1, &fBtn, rcInputBoxBtn, &sfCenter, &sbWhite);
				}

				break;
			}
			case 2: { // page 2 Paint
				Gdiplus::RectF rcMsg(0, rcCli.bottom / 3 - 50, rcCli.right, 100);
				g.FillRectangle(&sbTheme, rcMsg);
				g.DrawString(L"Game Starting...", -1, &fSubtitle, rcMsg, &sfCenter, &sbWhite);
				Sleep(1000);
				break;
			}
			case 3: { // page 3 Paint
				vmp.draw(g);

				Gdiplus::RectF rcTurn(0, 0, 100, 30);
				Font fTurn(L"等线", 12, FontStyleBold, UnitPoint, NULL);
				Gdiplus::SolidBrush sb1(0x80000000);
				std::wstringstream _tmp(L"");
				g.FillRectangle(&sb1, rcTurn);
				_tmp << L"Turn " << turn / 2 << (turn%2 ? L"." : L"");
				g.DrawString(_tmp.str().c_str(), -1, &fTurn, rcTurn, &sfCenter, &sbWhite);

				Gdiplus::RectF rcPlayer(rcCli.right - 220, 0, 100, 30);
				g.DrawString(L"Player", -1, &fTurn, rcPlayer, &sfCenter, &sbWhite);
				g.FillRectangle(&sb1, rcPlayer);
				Gdiplus::RectF rcArmy(rcCli.right - 120, 0, 60, 30);
				g.DrawString(L"Army", -1, &fTurn, rcArmy, &sfCenter, &sbWhite);
				g.FillRectangle(&sb1, rcArmy);
				Gdiplus::RectF rcLand(rcCli.right - 60, 0, 60, 30);
				g.DrawString(L"Land", -1, &fTurn, rcLand, &sfCenter, &sbWhite);
				g.FillRectangle(&sb1, rcLand);
				rcPlayer.Y = rcArmy.Y = rcLand.Y = 30;
				for(auto i : rank) {
					rcPlayer.Height = rcArmy.Height = rcLand.Height = -1;
					REAL maxHeight = -1;
					Gdiplus::RectF rcNow;
					Gdiplus::SolidBrush sbPlayer(PlayerColor[i.id]);
					g.MeasureString(player[i.id].name.c_str(), -1, &fTurn, rcPlayer, &sfCenter, &rcNow);
					maxHeight = max(maxHeight, rcNow.Height);
					std::wstringstream _tmp1(L"");
					_tmp1 << i.army;
					g.MeasureString(_tmp1.str().c_str(), -1, &fTurn, rcPlayer, &sfCenter, &rcNow);
					maxHeight = max(maxHeight, rcNow.Height);
					std::wstringstream _tmp2(L"");
					_tmp2 << i.land;
					g.MeasureString(_tmp2.str().c_str(), -1, &fTurn, rcPlayer, &sfCenter, &rcNow);
					maxHeight = max(maxHeight, rcNow.Height);
					rcPlayer.Height = rcArmy.Height = rcLand.Height = maxHeight;
					g.FillRectangle(&sb1, rcPlayer);
					g.FillRectangle(&sb1, rcArmy);
					g.FillRectangle(&sb1, rcLand);
					g.DrawString(player[i.id].name.c_str(), -1, &fTurn, rcPlayer, &sfCenter, &sbPlayer);
					g.DrawString(_tmp1.str().c_str(), -1, &fTurn, rcArmy, &sfCenter, &sbWhite);
					g.DrawString(_tmp2.str().c_str(), -1, &fTurn, rcLand, &sfCenter, &sbWhite);
					rcPlayer.Y = rcArmy.Y = rcLand.Y = rcPlayer.Y + maxHeight;
				}
				break;
			}
		}
		Gdiplus::CachedBitmap cb(&bmp, &gDest);
		gDest.DrawCachedBitmap(&cb, 0, 0);
		gDest.ReleaseHDC(hdc);
	}
	void Move(){

	}
	void Drag(HDC hdc, Gdiplus::Point pt) {
		RECT rcCli;
		GetClientRect(hwnd, &rcCli);
		bool isToPaint = false;
		switch(page) {
			case IN_GAME: { // page IN_GAME Drag

				if(abs(pt.X-lmx) + abs(pt.Y-lmy) <= 15) break;	//filter tiny drag
				ox += pt.X - lmx;
				oy += pt.Y - lmy;
				isToPaint = 1;
				
				break;
			}
		}
		if(isToPaint) Paint(hdc);
	}
	void Drop(HDC hdc, Gdiplus::Point pt) {
		RECT rcCli;
		GetClientRect(hwnd, &rcCli);
		bool isToPaint = false;
		switch(page) {
			case 3: { // page 3 Drop
				tab[0] = 0;
				break;
			}
		}
		if(isToPaint) Paint(hdc);
	}
	void Click(HDC hdc, Gdiplus::Point pt) {
		RECT rcCli;
		GetClientRect(hwnd, &rcCli);
		bool isToPaint = false;
		switch(page) {
			case 0: { // page 0 Click
				if(tab[0] <= 1) {
					Gdiplus::Rect rcPname(rcCli.right / 2 - 240, rcCli.bottom / 2, 480, 40);
					// Rect rcBtnPlay1(rcCli.right / 2 - 120, rcCli.bottom / 2 + 60, 240, 50);
					Gdiplus::Rect rcBtnPlay2(rcCli.right / 2 - 120, rcCli.bottom / 2 + 120, 240, 50);
					if(rcPname.Contains(pt)) {
						isToPaint |= tab[0] != 1;
						tab[0] = 1;
					} else {
						isToPaint |= tab[0] == 1;
						tab[0] = 0;
					}
					if(rcBtnPlay2.Contains(pt)) {
						tab[0] = (tab[0] != 2 ? 2 : 0);
						Network::sIP.clear();
						isToPaint = true;
					}
				} else if(tab[0] == 2) {
					Gdiplus::Rect rcInputBox(rcCli.right / 2 - 200, rcCli.bottom / 2 - 160, 400, 320);
					if(!rcInputBox.Contains(pt)) {
						tab[0] = 0;
						isToPaint = true;
					}
					Gdiplus::Rect rcInputBoxBtn(rcCli.right / 2 - 40, rcCli.bottom / 2 + 60, 80, 40);
					if(rcInputBoxBtn.Contains(pt)) {
						isToPaint |= startCli();
					}
				}
				break;
			}
			case 1: { // page 1 Click
				Gdiplus::Rect rcTeam(rcCli.right / 2 - 300, rcCli.bottom / 2 - 100, 150, 30);
				Gdiplus::Rect rcBack(rcCli.right / 2 - 220, rcCli.bottom / 2 + 200, 200, 30);
				Gdiplus::Rect rcStart(rcCli.right / 2 + 20, rcCli.bottom / 2 + 200, 200, 30);
				if(rcTeam.Contains(pt)) {
					isToPaint |= tab[0] != 0;
					tab[0] = 0;
				} 
				else if(rcBack.Contains(pt)) {
					isToPaint = 1;
					// closesocket(sockClient);
					setPage(0);
				} else if(rcStart.Contains((pt))) {
					isToPaint = 1;
					switchReady();
				}
				switch(tab[0]) {
					case 0: {
						for(int i = 1; i <= 12; i++) {
							Gdiplus::Rect rcInTeam(rcCli.right / 2 + (i - 7) * 60, rcCli.bottom / 2 - 60, 60, 30);
							if(rcInTeam.Contains(pt)) {
								if(me.team != i)
									isToPaint |= changeMyTeam(i);
								break;
							}
						}
						break;
					}
				}
				break;
			}
			case 3: { // page 3 Click
				curTile = { int((pt.X - ox) / (tileSize + 2)), int((pt.Y - oy) / tileSize) };
				isToPaint = 1;
				break;
			}
		}
		if(isToPaint) Paint(hdc);
	}

	void LButtonDown(HDC hdc, Gdiplus::Point pt) {
		RECT rcCli;		//Rectangle
		GetClientRect(hwnd, &rcCli);	//Get Client Rect
		bool isToPaint = false;		// will repaint
		switch(Game::page) {
			
			case IN_GAME: { // page 3 LButtonDown
				if(!(GetKeyState(VK_CONTROL) & 0x8000)) {
					curTile = { int((pt.X - ox) / (tileSize + 2)), int((pt.Y - oy) / tileSize) };
					tab[0] = 1;
					isToPaint = 1;
				}
				break;
			}
		}
		if(isToPaint) Paint(hdc);
	}
	void chInput(HDC hdc, wchar_t ch) {
		bool isToPaint = false;
		printf("in chInput event ch = %d\n", ch);
		switch(page) {
			case 0: {
				if(tab[0] == 1) {
					if(ch == 8) {
						if(me.name.size()) me.name.pop_back();
					} else
						me.name.push_back(ch);
					isToPaint = true;
				} else if(tab[0] == 2) {
					if(ch == 8) {
						if(Network::sIP.size()) Network::sIP.pop_back();
					} else if(ch == 13) {
						startCli();
					} else
						Network::sIP.push_back(ch);
					isToPaint = true;
				}
				break;
			}
		}
		if(isToPaint) Paint(hdc);
	}
	void onQuit(){

	}
}



LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	HDC hdc;	//painter?
	switch(Message) {
		// Launch when window is created
		case WM_CREATE: {
			srand(time(0));
			GdiplusStartup(&Gdip::m_gdiplusToken, &Gdip::StartupInput, NULL); 	//start up gdi+
			// if(WSAStartup(MAKEWORD(2, 2), &wsaData))		//start up winsock
			// 	MessageBox(hwnd, L"[ERROR] WSA startup failed.", titleAbbr, MB_ICONERROR);

			//import images
			Gdip::imgGeneral = Image::FromFile(L"./img/crown.png");
			Gdip::imgCity = Image::FromFile(L"./img/city.png");
			Gdip::imgMount = Image::FromFile(L"./img/mountain.png");
			Gdip::imgObstacle = Image::FromFile(L"./img/obstacle.png");
			Gdip::imgSwamp = Image::FromFile(L"./img/swamp.png");
			break;
		}
		// Launch when window is destroyed
		case WM_DESTROY: {
			Game::onQuit();
			PostQuitMessage(0);
			break;
		}

		case WM_PAINT: {
			hdc = BeginPaint(hwnd, &ps);
			Game::Paint(hdc);
			EndPaint(hwnd, &ps);
			break;
		}
		case WM_MOUSEMOVE: {
			if(Game::mousedown) {
				hdc = GetDC(hwnd);
				Game::Drag(hdc, Gdiplus::Point(LOWORD(lParam), HIWORD(lParam)));
				Game::lmx = LOWORD(lParam), Game::lmy = HIWORD(lParam);
				ReleaseDC(hwnd, hdc);
			}
			break;
		}
		case WM_LBUTTONDOWN: {
			Game::mousedownTime = clock();
			hdc = GetDC(hwnd);
			Game::lmx = LOWORD(lParam), Game::lmy = HIWORD(lParam);
			Game::mousedown = 1;
			Game::LButtonDown(hdc, Gdiplus::Point(LOWORD(lParam), HIWORD(lParam)));
			ReleaseDC(hwnd, hdc);
			break;
		}
		case WM_LBUTTONUP: {
			hdc = GetDC(hwnd);
			if(clock() - Game::mousedownTime <= 200) {	// a short click
				Game::Click(hdc, Gdiplus::Point(LOWORD(lParam), HIWORD(lParam)));
			} else {	// long drag
				Game::Drop(hdc, Gdiplus::Point(LOWORD(lParam), HIWORD(lParam)));
			}
			Game::mousedown = 0;
			ReleaseDC(hwnd, hdc);
			break;
		}
		case WM_CHAR: {		//get key event
			hdc = GetDC(hwnd);
			Game::chInput(hdc, wParam);
			ReleaseDC(hwnd, hdc);
			break;
		}
		case WM_SIZE: {		//resize
			hdc = GetDC(hwnd);
			Game::Paint(hdc);		//repaint
			ReleaseDC(hwnd, hdc);
			break;
		}
		// case WM_SOCKCLIENT: {	// a self-defined event, when  
		// 	hdc = GetDC(hwnd);
		// 	if(WSAGETSELECTEVENT(lParam) == FD_READ) {
		// 		int ret1 = recv((SOCKET)wParam, recvbuf, 2, 0);
		// 		if(ret1 <= 0) {
		// 			int _le = WSAGetLastError();
		// 			if(_le != WSAEWOULDBLOCK)
		// 				fprintf(stderr, "[WARNING] failed to receive. WSA error code: %d\n", _le);
		// 			break;
		// 		}
		// 		recv((SOCKET)wParam, recvbuf + 2, *(unsigned short *)recvbuf, 0);
		// 		if(side == -1) {
		// 			cliProcSockMsg(hdc);
		// 		}
		// 	} else if(WSAGETSELECTEVENT(lParam) == FD_CLOSE) {
		// 		cliSockClose(hdc);
		// 	}
		// 	ReleaseDC(hwnd, hdc);
		// 	break;
		// }
		case WM_MOUSEWHEEL: {
			hdc = GetDC(hwnd);
			if(Game::page == 3) {
				double newTileSize = Game::tileSize + GET_WHEEL_DELTA_WPARAM(wParam) / 120.0;
				if(newTileSize > 60 || newTileSize < 10)
					break;
				POINT _m = { LOWORD(lParam), HIWORD(lParam) };
				ScreenToClient(hwnd, &_m);
				Game::ox = _m.x - (_m.x - Game::ox) / (Game::tileSize + 2.0) * (newTileSize + 2.0);
				Game::oy = _m.y - (_m.y - Game::oy) / Game::tileSize * newTileSize;
				Game::tileSize = newTileSize;
				Game::Paint(hdc);
			}
			ReleaseDC(hwnd, hdc);
			break;
		}
		case WM_KEYDOWN: {
			hdc = GetDC(hwnd);
			bool isToPaint = false;
			if(Game::page == IN_GAME) {
				bool shiftKeyState = GetKeyState(VK_SHIFT) & 0x8000;
				if(wParam == VK_(D)) {
					isToPaint |= Game::vmp.pushMove(4 | shiftKeyState << 3);
				} else if(wParam == VK_(S)) {
					isToPaint |= Game::vmp.pushMove(5 | shiftKeyState << 3);
				} else if(wParam == VK_(A)) {
					isToPaint |= Game::vmp.pushMove(6 | shiftKeyState << 3);
				} else if(wParam == VK_(W)) {
					isToPaint |= Game::vmp.pushMove(7 | shiftKeyState << 3);
				} else if(wParam == VK_(E)) {
				} else if(wParam == VK_(Q)) {
				} else if(wParam == '1') {
					isToPaint |= Game::vmp.clearQue();
				} else if(wParam == '3') {
					isToPaint |= Game::vmp.undoMove();
				} else if(wParam == '5') {
					Game::scp=!Game::scp;
					isToPaint = true;
				}
			}
			if(isToPaint)
				Game::Paint(hdc);
			ReleaseDC(hwnd, hdc);
			break;
		}
		default:
			return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = wndClassName;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	if(!RegisterClassEx(&wc))
		return 1;
	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, wndClassName, titleAbbr, WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, NULL, NULL, hInstance, NULL);
	if(hwnd == NULL)
		return 2;
	while(GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}