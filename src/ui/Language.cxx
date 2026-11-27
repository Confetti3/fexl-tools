#include "ui/Language.hxx"

#include "Log.hxx"

#include <windows.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace fexl::ui {

namespace {

struct Translation {
  const char* english;
  const char* japanese;
};

// Fighting game terms as Japanese players use them: 発生 startup, 持続 active, 硬直 recovery,
// 喰らい判定 hurtbox, 攻撃判定 hitbox, のけぞり hitstun, ヒットストップ hit-freeze.
// ImGui wraps text at spaces only, so the longer Japanese texts carry their own line breaks.
constexpr Translation Japanese[] = {
    // The menu's frame.
    {"TRAINING TOOLS", "トレーニングツール"},
    {"HITBOXES", "判定"},
    {"FRAME METER", "フレームメーター"},
    {"PLAYBACK", "再生"},
    {"COLOUR KEY", "色の説明"},
    {"ABOUT", "情報"},
    {"ON", "オン"},
    {"OFF", "オフ"},
    {"Tab", "タブ"},
    {"Move", "移動"},
    {"Select", "決定"},
    {"Close", "閉じる"},
    {"Arrows", "矢印キー"},
    // Hitboxes.
    {"Show hitboxes", "判定を表示"},
    {"Spheres in the fight: where each fighter can be hit, and where attacks hit.", "各キャラの喰らい判定と、攻撃が当たる位置を球で表示します。"},
    {"WHAT TO SHOW", "表示する判定"},
    {"LOOK", "見た目"},
    {"Style", "スタイル"},
    {"Filled spheres need the engine's debug material, which is not loaded: outlines are drawn.",
     "塗りつぶしに必要なデバッグ素材が読み込まれていないため、輪郭のみ表示します。"},
    {"Filled spheres with an outline, outlines alone, or a wireframe.", "輪郭付きの塗りつぶし、輪郭のみ、またはワイヤーフレーム。"},
    {"Colours", "配色"},
    {"Ember's colours, a high-contrast set, or a set for colour-blind players.", "Ember標準、高コントラスト、または色覚多様性向けの配色。"},
    {"Fill opacity", "塗りの濃さ"},
    {"How solid the filled spheres are.", "塗りつぶしの濃さ。"},
    {"Outline width", "輪郭の太さ"},
    {"How thick each sphere's outline is.", "球の輪郭線の太さ。"},
    {"Draw over the fighters", "キャラの手前に表示"},
    {"Off: the fighters' bodies hide the spheres inside them.", "オフ: キャラの体の中の判定は体に隠れます。"},
    {"Filled", "塗りつぶし"},
    {"Outline", "輪郭"},
    {"Wireframe", "ワイヤーフレーム"},
    {"High contrast", "高コントラスト"},
    {"Colour-blind safe", "色覚多様性対応"},
    {"Hurtboxes", "喰らい判定"},
    {"Hurtbox: where strikes can hit the fighter.", "喰らい判定: 打撃を受ける範囲。"},
    {"Hitboxes", "攻撃判定"},
    {"Hitbox: an attack that can hit right now.", "攻撃判定: 今まさに当たる攻撃。"},
    {"Upcoming hitboxes", "発生前の攻撃判定"},
    {"Upcoming hitbox: where the attack will be once active.", "発生前の攻撃判定: 持続に入ったときの攻撃の位置。"},
    {"Throw hurtboxes", "投げ喰らい判定"},
    {"Throw hurtbox: where a throw can catch the fighter.", "投げ喰らい判定: 投げを受ける範囲。"},
    {"Projectiles", "飛び道具"},
    {"Projectile: fireballs and other thrown attacks.", "飛び道具: 飛び道具の攻撃判定。"},
    {"Invincible", "無敵"},
    {"Grey: invincible; that hurtbox cannot be hit right now.", "灰色: 無敵。今はその喰らい判定に攻撃が当たりません。"},
    // Frame meter.
    {"Show the frame meter", "フレームメーターを表示"},
    {"One cell per frame for each fighter, with the exchange's frame advantage.", "両キャラの状態を1フレーム1マスで表示し、攻防の硬直差を示します。"},
    {"Player 2's bar", "2Pのバー"},
    {"A bar for each fighter, or player 1's alone.", "両キャラのバー、または1Pのバーのみ。"},
    {"Frame data line", "フレームデータ"},
    {"Startup, active, recovery and total of the move, above the bars.", "技の発生・持続・硬直・全体フレームをバーの上に表示します。"},
    {"Colour key under the bars", "バーの下に色の説明"},
    {"Each colour's name under the meter. The Colour key tab explains them all.", "メーターの下に各色の名前を表示します。詳しくは「色の説明」タブへ。"},
    {"Controls line", "操作の表示"},
    {"The trainer's keys, at the top right of the meter.", "メーターの右上にツールのキーを表示します。"},
    {"Meter size", "メーターの大きさ"},
    {"How large the frame meter is drawn; 100% is the standard size.", "フレームメーターの表示サイズ。100%が標準です。"},
    {"Meter opacity", "メーターの濃さ"},
    {"How solid the frame meter is drawn; lower lets the fight show through it.", "フレームメーターの濃さ。下げると後ろの対戦画面が透けて見えます。"},
    {"At the top of the screen", "画面上部に表示"},
    {"Under the life bars instead of above the Gougi icons.", "豪技アイコンの上ではなく、体力ゲージの下に表示します。"},
    {"READING THE METER", "メーターの見方"},
    {"Each cell is one frame (1/60 s). A bar starts when either fighter commits and ends when both are free again. Hit-freeze "
     "is left out of the bars, as frame data leaves it out; a pink line marks where it was.",
     "1マスが1フレーム（1/60秒）です。どちらかが行動を始めるとバーが始まり、\n両者が自由に動けるようになると終わります。\n"
     "ヒットストップはフレームデータと同じくバーから除き、ピンクの線で位置を示します。"},
    {"On hit +1: once the exchange is over, the attacker can act one frame before the other fighter. Green is plus, red is "
     "minus.",
     "ヒット時 +1: 攻防の後、攻撃側が相手より1フレーム早く動けます。\n緑は有利、赤は不利です。"},
    {"Startup", "発生"},
    {"Active", "持続"},
    {"Recovery", "硬直"},
    {"Total", "全体"},
    {"On hit", "ヒット時"},
    {"On block", "ガード時"},
    {"knockdown", "ダウン"},
    {"hits", "ヒット"},
    {"No contact", "空振り"},
    {"Throw connected", "投げ成立"},
    {"Frame data shows once a move is active", "技の持続に入るとフレームデータを表示します"},
    {"The move is coming out; it cannot hit yet.", "技の出始め。まだ当たりません。"},
    {"The move can hit on these frames.", "攻撃が当たるフレーム。"},
    {"After the hit window; the fighter cannot act yet.", "攻撃後の隙。まだ行動できません。"},
    {"Hit-freeze", "ヒットストップ"},
    {"Both fighters freeze on contact; not counted, shown as a pink line.",
     "当たった瞬間に両者が止まる時間。バーから除き、ピンクの線で示します。"},
    {"Hitstun", "のけぞり"},
    {"Being hit or thrown; cannot act.", "攻撃を受けている、または投げられている状態。行動できません。"},
    {"Blockstun", "ガード硬直"},
    {"Blocking; cannot act.", "ガード中。行動できません。"},
    {"Guard pose", "ガード姿勢"},
    {"Ready to block as an attack comes; free to act.", "攻撃に備えたガード姿勢。行動できます。"},
    {"Movement", "移動"},
    {"Walking or dashing.", "歩き・ダッシュ。"},
    {"Jump", "ジャンプ"},
    {"A jump with no attack: take-off, the air and the landing frame.", "攻撃を出さないジャンプ。踏み切り、空中、着地のフレーム。"},
    {"Take-off", "踏み切り"},
    {"Airborne", "空中"},
    {"Landing", "着地"},
    {"Teleport", "テレポート"},
    {"Warping to another place; cannot act.", "別の場所へ瞬間移動中。行動できません。"},
    {"Knockdown", "ダウン"},
    {"Knocked down or getting up.", "ダウン中・起き上がり中。"},
    {"Throw", "投げ"},
    {"A throw that caught: holding and throwing the opponent.", "投げが決まり、相手をつかんで投げている間。"},
    {"Other", "その他"},
    {"States not identified yet.", "まだ識別できない状態。"},
    {"Neither strikes nor throws can touch the fighter.", "打撃も投げも当たりません。"},
    {"Strike-proof", "打撃無敵"},
    {"Strikes cannot hit the fighter.", "打撃が当たりません。"},
    {"Throw-proof", "投げ無敵"},
    {"Throws cannot catch the fighter.", "投げが当たりません。"},
    // Playback.
    {"Pause the fight", "試合を一時停止"},
    {"Stops the fight; the camera and menus keep running.", "試合を止めます。カメラとメニューは動き続けます。"},
    {"Step one frame", "1フレーム進める"},
    {"Runs one frame of a held fight. To press a button on that frame, close this menu and hold it through F8.",
     "止めた試合を1フレーム進めます。そのフレームでボタンを押すには、\nメニューを閉じてボタンを押したままF8を押します。"},
    {"STEP", "進める"},
    {"Pause while this menu is open", "メニュー表示中は一時停止"},
    {"The fight waits while you change settings.", "設定を変えている間、試合を止めます。"},
    {"Speed", "速度"},
    {"Slow motion for the fight only; menus and the camera keep running.", "試合のみスローにします。メニューとカメラは通常の速さです。"},
    {"Normal speed", "通常速度"},
    {"Half speed", "1/2速"},
    {"Quarter speed", "1/4速"},
    {"PAUSED", "一時停止中"},
    {"Training tools", "トレーニングツール"},
    {"Meter", "メーター"},
    {"Hitbox view", "判定表示"},
    {"Pause", "一時停止"},
    {"Step", "コマ送り"},
    {"Resume", "再開"},
    // Colour key and About.
    {"What every colour on the frame meter and the hitboxes means. Up and down scroll the page.",
     "フレームメーターと判定の各色の意味。上下でスクロールします。"},
    {"MARKS ABOVE A CELL", "マスの上の印"},
    {"An attack hits when its hitbox touches the other fighter's hurtbox.", "攻撃判定が相手の喰らい判定に触れると、攻撃がヒットします。"},
    {"The trainer's controls. Up and down scroll the page.", "ツールの操作方法。上下でスクロールします。"},
    {"FEXL TRAINING TOOLS 0.8", "FEXL トレーニングツール 0.8"},
    {"For FIGHTING EX LAYER on Steam, version 1.6.0. Works in Training mode only: nothing shows, and nothing changes, in "
     "online or other matches. In Training it draws over the fight, and holds or slows it only when you ask.",
     "Steam版 FIGHTING EX LAYER（バージョン1.6.0）用。トレーニングモード専用です。\n"
     "オンラインや他のモードでは何も表示せず、何も変更しません。\n"
     "トレーニングでは試合の上に表示し、指示したときだけ試合を止めたり遅くしたりします。"},
    {"PAD, WHILE THIS MENU IS OPEN", "パッド（メニュー表示中）"},
    {"Change tab", "タブ切り替え"},
    {"Move, and change a value with left and right", "移動、左右で値を変更"},
    {"Change the setting", "設定を変更"},
    {"KEYBOARD", "キーボード"},
    {"Training tools menu", "トレーニングツールのメニュー"},
    {"Frame meter on or off", "フレームメーターの表示切り替え"},
    {"Hitboxes on or off", "判定の表示切り替え"},
    {"Pause or resume the fight", "試合の一時停止・再開"},
    {"Step one frame while paused", "一時停止中に1フレーム進める"},
    {"LANGUAGE", "言語"},
    {"Language", "言語"},
    {"The trainer's language. Auto follows Windows' display language.", "ツールの表示言語。自動はWindowsの表示言語に合わせます。"},
    {"Japanese needs a Japanese font (Yu Gothic, Meiryo or MS Gothic), which was not found.",
     "日本語の表示には日本語フォント（游ゴシック、メイリオ、MS ゴシック）が必要ですが、見つかりませんでした。"},
    {"Auto", "自動"},
    // Names that stay as they are.
    {"English", "English"},
    {"日本語", "日本語"},
    {"Ember", "Ember"},
    {"Enter", "Enter"},
    {"Esc", "Esc"},
    {"Q", "Q"},
    {"E", "E"},
    {"Q/E", "Q/E"},
    {"F1", "F1"},
    {"F5", "F5"},
    {"F6", "F6"},
    {"F7", "F7"},
    {"F8", "F8"},
    {"P2", "P2"},
};

bool s_japanese;

const std::unordered_map<std::string_view, const char*>& Table() {
  static const auto table = [] {
    std::unordered_map<std::string_view, const char*> map;
    for (const Translation& t : Japanese) map.emplace(t.english, t.japanese);
    return map;
  }();
  return table;
}

bool WindowsIsJapanese() { return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_JAPANESE; }

}  // namespace

void UseLanguage(Language language, bool japaneseFont) {
  const bool wanted = language == Language::Japanese || (language == Language::Auto && WindowsIsJapanese());
  s_japanese = wanted && japaneseFont;
}

const char* T(const char* english) {
  if (!s_japanese || !english || !*english) return english;
  const auto& table = Table();
  if (const auto found = table.find(english); found != table.end()) return found->second;
  // Every English string drawn has an entry; one without is named once.
  static std::unordered_set<std::string> named;
  if (named.insert(english).second) Log("language: no Japanese for \"%s\"", english);
  return english;
}

void AddJapaneseGlyphs(ImFontGlyphRangesBuilder& builder) {
  for (const Translation& t : Japanese) builder.AddText(t.japanese);
}

}  // namespace fexl::ui
