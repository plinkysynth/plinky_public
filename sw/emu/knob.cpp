#include "imgui.h"
#include <math.h>
#include <stdio.h>
#define PI 3.1415926535897932384626433832795f
typedef unsigned int u32;
 
inline float lerp(float a, float b, float t) { return a+(b-a)*t; }
inline float saturate(float a) { return (a<0.f)?0.f:(a>1.f)?1.f:a; }
inline float length(float x, float y ){ return sqrtf(x*x+y*y); }

int TactileButton(const char *label, float size) {
	ImVec4 hilitecol = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
	hilitecol.x+=0.1f;
	auto style=ImGui::GetStyle();		
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 mouse_pos = ImGui::GetIO().MousePos;
	ImVec2 canvas_pos = ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!
	float w=size,/*ImGui::CalcItemWidth(),*/ h=size;
	ImVec2 canvas_size = ImVec2(w,h);
	ImVec2 centre=ImVec2(canvas_pos.x+w*0.5f,canvas_pos.y+h*0.45f);
	ImGui::InvisibleButton(label, canvas_size);
	draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x+canvas_size.x, canvas_pos.y+canvas_size.y),true);      // clip lines within the canvas (if we resize it, etc.)
	bool active=ImGui::IsItemActive();
	draw_list->AddCircleFilled(ImVec2(centre.x,centre.y),h*0.45f,active ? 0x80ffffff : 0x40ffffff, 32);
	ImVec2 labelsize=ImGui::CalcTextSize(label);
	draw_list->AddText(ImVec2(centre.x-labelsize.x*0.5f,canvas_pos.y-labelsize.y+h*0.5f),ImColor(style.Colors[ImGuiCol_Text]),label);
	draw_list->PopClipRect();		
	return active;
}

int Knob(const char *label, float &curval, float *randamount, float minval, float maxval, u32 encodercol, float size)
{

	ImVec4 hilitecol = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
	hilitecol.x+=0.1f*(curval-minval)/(maxval-minval);
//	hilitecol.z-=0.3f;
	static ImVec2 click_pos;	
	static float click_val,click_rand;
	static int grabrand;
	static bool clickedbutton;
	auto style=ImGui::GetStyle();		
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 mouse_pos = ImGui::GetIO().MousePos;
	ImVec2 canvas_pos = ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!
	float w=size,/*ImGui::CalcItemWidth(),*/ h=size;
	ImVec2 canvas_size = ImVec2(w,h);
	ImVec2 centre=ImVec2(canvas_pos.x+w*0.5f,canvas_pos.y+h*0.45f);
	ImGui::InvisibleButton(label, canvas_size);
	draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x+canvas_size.x, canvas_pos.y+canvas_size.y),true);      // clip lines within the canvas (if we resize it, etc.)
	float rad=h*0.4f;
	ImColor bg= hilitecol;// : style.Colors[ImGuiCol_FrameBg];	
	u32 tickcol=0x80ffffff;
	if (encodercol==1)
		encodercol=bg;
	float angrange=encodercol ? 3.141592f : 2.5f;
	float curang=lerp(-angrange,angrange,(curval-minval)/(maxval-minval));

	draw_list->PathClear();
	draw_list->PathArcTo(centre,rad,-angrange - PI/2,angrange - PI/2,20);
	draw_list->PathStroke(bg,false,rad*0.2f);
	bool active=ImGui::IsItemActive();
	if (active && ImGui::IsMouseClicked(0)) { 
		clickedbutton=false;
		click_pos=mouse_pos; click_val=curval; click_rand=randamount ? *randamount : 0.f; 
		grabrand=randamount && ImGui::GetIO().KeyShift;
		if (length(mouse_pos.x-centre.x, mouse_pos.y-centre.y)<h*0.3f)
			clickedbutton=true;
	}	

	if (!encodercol) {
		draw_list->PathClear();
		draw_list->PathArcTo(centre,rad,-angrange-PI/2,curang-PI/2,20);
		draw_list->PathStroke(tickcol,false,rad*0.2f);
	} else {
		draw_list->AddCircleFilled(ImVec2(centre.x,centre.y),h*0.3f,active && clickedbutton ? tickcol : encodercol, 32);
	}
	draw_list->AddCircleFilled(ImVec2(centre.x+0.f,centre.y-h*0.48f),h*0.02f,tickcol);

	ImColor barcol=(style.Colors[ImGuiCol_SliderGrabActive]); //:(i==hover)?ImGuiCol_CheckMark:ImGuiCol_ScrollbarGrab]))
	if (randamount && *randamount>0.f) {
		float minang=curang-angrange**randamount; if (minang<-angrange) minang=-angrange;
		float maxang=minang+angrange**randamount; if (maxang>angrange) { maxang=angrange; minang=maxang-angrange**randamount*2.f; }
		draw_list->PathClear();
		draw_list->PathArcTo(centre,rad,minang,maxang,20);
		draw_list->PathStroke(barcol,false,rad*0.05f);
	}
	float rad0=rad*0.4f;
	float rad2=rad+0.05f*h;
	draw_list->AddLine(ImVec2(centre.x+rad0*sinf(curang),centre.y-rad0*cosf(curang)), ImVec2(centre.x+rad2*sinf(curang),centre.y-rad2*cosf(curang)),tickcol, 2.f);
	int rv=0;
	if (active) {
		float angdelta = -atan2f(mouse_pos.x-centre.x, mouse_pos.y-centre.y) + atan2f(click_pos.x-centre.x,click_pos.y-centre.y);
//		if (clickedbutton)
//			angdelta=0.f;
		if (angdelta>PI) angdelta-=PI*2.f;
		if (angdelta<-PI) angdelta+=PI*2.f;
		angdelta*=1.f/(angrange*2.f);
//		ImVec2 delta(mouse_pos.x-click_pos.x,mouse_pos.y-click_pos.y);
//		float angdelta=((fabsf(delta.x)>fabsf(delta.y)) ? delta.x : -delta.y) / h;
		if (grabrand && randamount) 
			*randamount=saturate(click_rand+angdelta);
		else
		{
			curval=click_val+angdelta*(maxval-minval);
			if (!encodercol) {
				if (curval<minval) curval=minval;
				if (curval>maxval) curval=maxval;
			}
		}
		click_pos=mouse_pos; click_val=curval; click_rand=randamount ? *randamount : 0.f; 
	}
	draw_list->PopClipRect();		

//	char valuebuf[64];
	//snprintf(valuebuf,sizeof(valuebuf),"%.1f%%",curval*100.f);
   ImVec2 labelsize=ImGui::CalcTextSize(label);
//   ImVec2 valuesize=ImGui::CalcTextSize(valuebuf);

	draw_list->AddText(ImVec2(centre.x-labelsize.x*0.5f,canvas_pos.y-labelsize.y+h*0.5f),ImColor(style.Colors[ImGuiCol_Text]),label);
//	if (!encodercol)
//		draw_list->AddText(ImVec2(centre.x-valuesize.x*0.5f,canvas_pos.y+canvas_size.y-valuesize.y-2.f),ImColor(style.Colors[ImGuiCol_Text]),valuebuf);
	return clickedbutton && active;
}
