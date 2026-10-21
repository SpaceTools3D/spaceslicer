/*---- SpaceSlicer home page. The app talks to this page through HandleStudio (window.postMessage)
      and a few direct calls (SetLoginPanelVisibility, SetCloudMode); the page answers with
      SendWXMessage JSON commands handled in GUI_App.cpp. ----*/

var SPACETOOLS_URL = "https://spacetools3d.com/";
var HOME_RECENT_COUNT = 12;  /*----enough to fill the home board's single row on wide screens----*/

function OnInit()
{
	TranslatePage();
	let pluginTip = document.getElementById("NoPluginText");
	if (pluginTip)
		AppendTemplate(pluginTip, T("plugin_not_detected", "Bambu Network plug-in not detected. Click <a>here</a> to install it."),
		               pluginTip.querySelector("a"));

	SendMsg_GetLoginInfo();
	SendMsg_GetBambuLoginInfo();
	SendMsg_GetRecentFile();

	Set_AccountMenu_Event();
	Set_RecentFile_Events();
	UpdateRecentClearBtnDisplay();
	ShowFeed("library", []);
	ShowFeed("gallery", []);
}

var RightBtnFilePath = '';
var MousePosX = 0;
var MousePosY = 0;
var sImages = {};
var m_CurrentMenu = "home";
var m_SearchText = "";

function SetLoginPanelVisibility(visible) {
	document.getElementById("LeftBoard").style.display = "flex";
}

function HandleStudio( pVal )
{
	let strCmd = pVal['command'];

	if (strCmd == "get_recent_projects") {
		ShowRecentFileList(pVal["response"]);
	} else if (strCmd == "orca_userlogin") {
		SetOrcaLoginInfo(pVal["data"]["avatar"], pVal["data"]["name"], pVal["data"]["account"]);
	} else if (strCmd == "orca_useroffline") {
		SetOrcaUserOffline();
	} else if (strCmd == "studio_bambu_userlogin") {
		SetBambuLoginInfo(pVal["data"]["avatar"], pVal["data"]["name"]);
	} else if (strCmd == "studio_bambu_useroffline") {
		SetBambuUserOffline();
	} else if (strCmd == "studio_clickmenu") {
		GotoMenu(pVal["data"]["menu"]);
	} else if (strCmd == "cloud_providers_info") {
		var providers = (pVal["data"] && pVal["data"]["providers"]) || [];

		/*----the side bar is SpaceSlicer's own navigation, so it shows whatever cloud providers exist----*/
		if (providers.indexOf("bbl") >= 0) {
			$("#BambuCloudSection").show();
		} else {
			SetBambuUserOffline();
			$("#BambuCloudSection").hide();
		}
	} else if (strCmd == "network_plugin_installtip") {
		// Bambu Cloud is unreachable without the network plugin, so its row only carries the tip.
		let bMissing = pVal["show"] * 1 == 1;

		$("#NoPluginTip").css("display", bMissing ? "block" : "none");
		$("#BambuAccount").toggleClass("Disabled", bMissing).attr("aria-disabled", bMissing ? "true" : null);
		if (bMissing) SetBambuSectionExpanded(true, false);
	} else if (strCmd == "spacetools_feed") {
		ShowFeed(pVal["data"]["kind"], pVal["data"]["items"]);
	} else if (strCmd == "SetLoginPanelVisibility") {
		SetLoginPanelVisibility(pVal["data"]["visible"]);
	}
}

/*---- The cloud pane opens natively beside this page (WebViewPanel::set_cloud_mode); the page
      then shrinks to its side bar. ----*/
var m_CloudMode = false;

function SetCloudMode( bOn )
{
	if (m_CloudMode == bOn)
		return;

	m_CloudMode = bOn;
	$("body").toggleClass("cloud-mode", bOn);
}

function GotoMenu( strMenu )
{
	if ($("div[board='" + strMenu + "']").length == 0)
		strMenu = (strMenu == "recent") ? "recent" : "home";

	if (m_CloudMode)
	{
		SetCloudMode(false);
		SendSimpleCommand("homepage_close_cloud");
	}

	m_CurrentMenu = strMenu;
	$("#BtnArea .BtnItem").each(function () {
		$(this).toggleClass("BtnItemSelected", $(this).attr("menu") == strMenu);
	});
	$("div[board]").each(function () {
		this.hidden = $(this).attr("board") != strMenu;
	});
	ApplySearch();
}

/*------Search: filters the projects and models of the open board by name------*/
function OnSearchInput( strText )
{
	m_SearchText = strText.trim().toLocaleLowerCase();
	ApplySearch();
}

function ApplySearch()
{
	let board = $("div[board='" + m_CurrentMenu + "']");
	let items = board.find(".FileItem, .ModelCard");
	let nShown = 0;
	$(".FileItem, .ModelCard").each(function () {
		let bMatch = m_SearchText === "" || ($(this).attr("data-name") || "").indexOf(m_SearchText) >= 0;
		this.hidden = !bMatch;
		if (bMatch && board.has(this).length) nShown++;
	});
	/*----while searching, the home board shows every match instead of one row per section,
	      and only the sections that have one----*/
	$("body").toggleClass("Searching", m_SearchText !== "");
	$("[board=home] .Section").each(function () {
		this.hidden = m_SearchText !== "" && $(this).find(".FileItem, .ModelCard").filter(function () { return !this.hidden; }).length == 0;
	});
	document.getElementById("NoResults").hidden = !(m_SearchText !== "" && items.length > 0 && nShown == 0);
}

/*------Recent projects------*/
function EscapeHtml( str )
{
	return String(str == null ? "" : str).replace(/[&<>"']/g, function (c) {
		return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
	});
}

function FileNameOf( sPath )
{
	let s = String(sPath || "");
	let n = Math.max(s.lastIndexOf('\\'), s.lastIndexOf('/'));
	return s.substring(n + 1).replace(/\.3mf$/i, "");
}

function RecentFileCard( OneFile )
{
	let sPath = OneFile['path'];
	let sImg = OneFile["image"] || sImages[sPath];
	let sName = OneFile['project_name'] || FileNameOf(sPath);
	sImages[sPath] = sImg;

	let sThumb = sImg ? '<img src="' + EscapeHtml(sImg) + '" onerror="this.remove()" alt="" />' : '';
	let sBadge = OneFile['published'] == '1' ? '<span class="FilePublishedBadge">' + EscapeHtml(T("published_badge", "PUB")) + '</span>' : '';

	return '<div class="FileItem" tabindex="0" fpath="' + EscapeHtml(sPath) + '" data-name="' + EscapeHtml(String(sName).toLocaleLowerCase()) + '" title="' + EscapeHtml(sPath) + '">' +
		'<div class="FileImg">' + sThumb + sBadge + '</div>' +
		'<div class="FileName">' + EscapeHtml(sName) + '</div>' +
		'<div class="FileDate">' + EscapeHtml(OneFile['time']) + '</div>' +
		'</div>';
}

function ShowRecentFileList( pList )
{
	pList = pList || [];
	let strAll = '';
	let strHome = '';
	for (let n = 0; n < pList.length; n++)
	{
		let card = RecentFileCard(pList[n]);
		strAll += card;
		if (n < HOME_RECENT_COUNT) strHome += card;
	}

	$("#FileList").html(strAll);
	$("#HomeFileList").html(strHome);

	UpdateRecentClearBtnDisplay();
	ApplySearch();
}

function Set_RecentFile_Events()
{
	$(document).on("mousedown", ".FileItem", function (e) {
		RightBtnFilePath = $(this).attr('fpath');
		if (e.which == 3)
			ShowRecnetFileContextMenu();
		else if (e.which == 1)
			OnOpenRecentFile(encodeURI(RightBtnFilePath));
	});

	$(document).on("click", ".ModelCard", function () { OpenModelCard(this); });
	$(document).on("keydown", ".ModelCard", function (e) { if (e.key === "Enter") OpenModelCard(this); });

	$(document).on("keydown", ".FileItem", function (e) {
		if (e.key !== "Enter") return;
		RightBtnFilePath = $(this).attr('fpath');
		OnOpenRecentFile(encodeURI(RightBtnFilePath));
	});

	$(document).bind("contextmenu", function (e) {
		return false;
	});

	$(document).mousemove(function (e) {
		MousePosX = e.pageX;
		MousePosY = e.pageY;
	});

	$(document).click(function (e) {
		if ($(e.target).closest("#recnet_context_menu").length) return;
		$("#recnet_context_menu").hide();
	});
}

function ShowRecnetFileContextMenu()
{
	let menu = $("#recnet_context_menu");
	menu.css({ top: "-10000px", left: "-10000px" }).show();

	let RealX = Math.min(MousePosX, $(window).width() - menu.outerWidth() - 12);
	let RealY = Math.min(MousePosY, $(window).height() - menu.outerHeight() - 12);
	menu.css({ top: RealY + "px", left: RealX + "px" });
}

function UpdateRecentClearBtnDisplay()
{
	let bAny = $("#FileList .FileItem").length > 0;
	$("#RecentClearAllBtn").toggle(bAny);
	$(".RecentEmpty").each(function () { this.hidden = bAny; });
	$("[board=home] .SeeAll[onClick*=recent]").toggle(bAny);
	$("#HomeFileList").toggle(bAny);
}

function OnDeleteRecentFile()
{
	$("#recnet_context_menu").hide();

	$(".FileItem").filter(function () { return this.getAttribute("fpath") == RightBtnFilePath; }).remove();
	UpdateRecentClearBtnDisplay();

	var tSend = {};
	tSend['sequence_id'] = Math.round(new Date() / 1000);
	tSend['command'] = "homepage_delete_recentfile";
	tSend['data'] = {};
	tSend['data']['path'] = RightBtnFilePath;

	SendWXMessage(JSON.stringify(tSend));
}

function OnDeleteAllRecentFiles()
{
	$('#FileList, #HomeFileList').html('');
	UpdateRecentClearBtnDisplay();
	ApplySearch();

	SendSimpleCommand("homepage_delete_all_recentfile");
}

function OnExploreRecentFile()
{
	var tSend = {};
	tSend['sequence_id'] = Math.round(new Date() / 1000);
	tSend['command'] = "homepage_explore_recentfile";
	tSend['data'] = {};
	tSend['data']['path'] = decodeURI(RightBtnFilePath);

	SendWXMessage(JSON.stringify(tSend));

	$("#recnet_context_menu").hide();
}

/*------SpaceTools3D web pages open in the browser until the account sign-in lands in the app------*/
function OpenUrl( strUrl )
{
	var tSend = {};
	tSend['sequence_id'] = Math.round(new Date() / 1000);
	tSend['command'] = "common_openurl";
	tSend['url'] = strUrl;

	SendWXMessage(JSON.stringify(tSend));
}

function OpenSpaceTools( strPath ) { OpenUrl(SPACETOOLS_URL + (strPath || "")); }

/*------Model library and gallery: cards from the SpaceTools3D account (spacetools_feed). Until the
      app sends a feed, each section shows a card that opens the site instead.------*/
function FormatCount( n )
{
	n = Number(n) || 0;
	return n >= 1000 ? (Math.round(n / 100) / 10).toString().replace(/\.0$/, "") + "k" : String(n);
}

function ModelCard( item )
{
	let sTitle = item["title"] || "";
	let sAuthor = item["author"] ? '<span class="CardAuthor"><span class="CardAvatar">' +
		(item["author_avatar"] ? '<img src="' + EscapeHtml(item["author_avatar"]) + '" onerror="this.remove()" alt="" />' : '') +
		'</span><span class="CardAuthorName">' + EscapeHtml(item["author"]) + '</span></span>' : '<span class="CardAuthor"></span>';
	let sStats = '';
	if (item["downloads"] != null)
		sStats += '<span class="CardStat"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 4v11M7.5 10.5L12 15l4.5-4.5M5 19.5h14"/></svg>' + FormatCount(item["downloads"]) + '</span>';
	if (item["likes"] != null)
		sStats += '<span class="CardStat"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 19.5s-7.5-4.4-7.5-9.6A4.1 4.1 0 0 1 12 7.6a4.1 4.1 0 0 1 7.5 2.3c0 5.2-7.5 9.6-7.5 9.6z"/></svg>' + FormatCount(item["likes"]) + '</span>';

	return '<div class="ModelCard" tabindex="0" data-url="' + EscapeHtml(item["url"] || "") + '" data-name="' + EscapeHtml(sTitle.toLocaleLowerCase()) + '" title="' + EscapeHtml(sTitle) + '">' +
		'<div class="CardImg">' + (item["image"] ? '<img src="' + EscapeHtml(item["image"]) + '" onerror="this.remove()" alt="" />' : '') + '</div>' +
		'<div class="CardBody"><div class="CardTitle">' + EscapeHtml(sTitle) + '</div>' +
		'<div class="CardMeta">' + sAuthor + sStats + '</div></div>' +
		'</div>';
}

function ShowFeed( strKind, items )
{
	items = items || [];
	let html = items.map(ModelCard).join("");
	$("[data-feed='" + strKind + "']").html(html);
	$("[data-empty='" + strKind + "']").each(function () { this.hidden = items.length > 0; });
	$("[board=home] .SeeAll[onClick*=" + strKind + "]").toggle(items.length > 0);
	ApplySearch();
}

function OpenModelCard( card )
{
	let url = $(card).attr("data-url") || "";
	/*----only SpaceTools3D pages open from a card----*/
	if (url.indexOf(SPACETOOLS_URL) === 0)
		OpenUrl(url);
}

/*------Account rows------*/

var AccountRows = {
  orca:  { row: "#OrcaAccount",  name: "#UserName",      avatar: "#UserAvatarIcon",  dot: null,
           loginCmd: "homepage_orca_login_or_register",  logoutCmd: "homepage_orca_logout" },
  bambu: { row: "#BambuAccount", name: "#BambuUserName", avatar: "#BambuAvatarIcon", dot: "#BambuStatusDot",
           loginCmd: "homepage_bambu_login_or_register", logoutCmd: "homepage_bambu_logout" }
};

var m_OpenAccountMenu = null;

function SetAccountAvatar(selector, strAvatar) {
  if (strAvatar != null && strAvatar.trim() !== '') {
    if ($(selector).attr("src") !== strAvatar) $(selector).attr("src", strAvatar);
  } else {
    $(selector).removeAttr("src");
  }
}

function OnAvatarLoadError(img) {
  img.removeAttribute("src");  // fall back to the placeholder glyph
}

function SetAccountSignedIn(strProvider, strAvatar, strName, strHandle) {
  var account = AccountRows[strProvider];

  $(account.name).text(strName).attr("title", strName);
  SetAccountAvatar(account.avatar, strAvatar);
  $(account.row).addClass("SignedIn").data("handle", strHandle !== strName ? strHandle : null);
  $(account.dot).addClass("Online");

  if (m_OpenAccountMenu === strProvider) OpenAccountMenu(strProvider);
}

function SetAccountSignedOut(strProvider) {
  var account = AccountRows[strProvider];

  if (m_OpenAccountMenu === strProvider) CloseAccountMenu();
  $(account.name).text('').removeAttr("title");
  SetAccountAvatar(account.avatar, null);
  $(account.row).removeClass("SignedIn").removeData("handle");
  $(account.dot).removeClass("Online");
}

function OnAccountRowClick(strProvider) {
  var account = AccountRows[strProvider];
  var row = $(account.row);
  if (row.hasClass("Disabled")) return;

  if (!row.hasClass("SignedIn")) {
    CloseAccountMenu();
    SendSimpleCommand(account.loginCmd);
  } else if (m_OpenAccountMenu === strProvider) {
    CloseAccountMenu();
  } else {
    OpenAccountMenu(strProvider);
  }
}

function OpenAccountMenu(strProvider) {
  var account = AccountRows[strProvider];
  var row = $(account.row);
  var top = row[0].offsetTop + row[0].offsetHeight + 4;  /*----read the geometry before writing----*/
  var strHandle = row.data("handle") || "";

  $("#AccountMenuHandle").text(strHandle).css("display", strHandle === "" ? "none" : "block");
  $("#AccountMenu").css("top", top + "px").addClass("Open");
  $(".AccountRow").removeClass("MenuOpen").attr("aria-expanded", "false");
  row.addClass("MenuOpen").attr("aria-expanded", "true");
  m_OpenAccountMenu = strProvider;
}

function CloseAccountMenu() {
  $("#AccountMenu").removeClass("Open");
  $(".AccountRow").removeClass("MenuOpen").attr("aria-expanded", "false");
  m_OpenAccountMenu = null;
}

function OnAccountMenuLogout() {
  var account = AccountRows[m_OpenAccountMenu];
  CloseAccountMenu();
  if (account) SendSimpleCommand(account.logoutCmd);
}

/*----the Bambu account is the side bar's account row, always open; kept for the plug-in tip----*/
function SetBambuSectionExpanded(bExpanded, bPersist) {}

function Set_AccountMenu_Event() {
  $(document).mousedown(function (e) {
    if (m_OpenAccountMenu === null) return;
    if ($(e.target).closest("#AccountMenu, .AccountRow").length === 0) CloseAccountMenu();
  });

  $(document).keydown(function (e) {
    if (m_OpenAccountMenu !== null && e.key === "Escape") CloseAccountMenu();
  });

  /*----Enter and Space activate the div-based buttons of the side bar----*/
  $(document).on("keydown", "#LeftBoard [role=button], #LeftBoard [role=menuitem], .SeeAll", function (e) {
    if (e.key !== "Enter" && e.key !== " " && e.key !== "Spacebar") return;
    e.preventDefault();
    this.click();
  });
}

function SetOrcaLoginInfo(strAvatar, strName, strAccount) { SetAccountSignedIn("orca", strAvatar, strName, strAccount); }
function SetOrcaUserOffline() { SetAccountSignedOut("orca"); }
function SetBambuLoginInfo(strAvatar, strName) { SetAccountSignedIn("bambu", strAvatar, strName, null); }
function SetBambuUserOffline() { SetAccountSignedOut("bambu"); }

/*-------Messages to the app------*/
function SendSimpleCommand(command) {
  var tSend = {};
  tSend['sequence_id'] = Math.round(new Date() / 1000);
  tSend['command'] = command;
  SendWXMessage(JSON.stringify(tSend));
}

function SendMsg_GetLoginInfo() { SendSimpleCommand("get_login_info"); }
function SendMsg_GetOrcaLoginInfo() { SendSimpleCommand("get_orca_login_info"); }
function SendMsg_GetBambuLoginInfo() { SendSimpleCommand("get_bambu_login_info"); }
function SendMsg_GetRecentFile() { SendSimpleCommand("get_recent_projects"); }
function OnClickNewProject() { SendSimpleCommand("homepage_newproject"); }
function OnClickOpenProject() { SendSimpleCommand("homepage_openproject"); }
function BeginDownloadNetworkPlugin() { SendSimpleCommand("begin_network_plugin_download"); }

function OnOpenRecentFile( strPath )
{
	var tSend = {};
	tSend['sequence_id'] = Math.round(new Date() / 1000);
	tSend['command'] = "homepage_open_recentfile";
	tSend['data'] = {};
	tSend['data']['path'] = decodeURI(strPath);

	SendWXMessage(JSON.stringify(tSend));
}

function OutputKey(keyCode, isCtrlDown, isShiftDown, isCmdDown) {
	var tSend = {};
	tSend['sequence_id'] = Math.round(new Date() / 1000);
	tSend['command'] = "get_web_shortcut";
	tSend['key_event'] = {};
	tSend['key_event']['key'] = keyCode;
	tSend['key_event']['ctrl'] = isCtrlDown;
	tSend['key_event']['shift'] = isShiftDown;
	tSend['key_event']['cmd'] = isCmdDown;

	SendWXMessage(JSON.stringify(tSend));
}

//---------------Global-----------------
window.postMessage = HandleStudio;
