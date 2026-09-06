import { analyzeLog, MAX_LOG_BYTES } from './diagnostics.mjs';
import { plainCards, plainSecurity, plainRecovery, nextAction, supportSummary, connectionPicture } from './presentation.mjs';
const $ = id => document.getElementById(id);
const node = (tag, text, className) => { const n = document.createElement(tag); n.textContent = text; if (className) n.className = className; return n; };
let port, reader, reading = false, opening = false, lastSnapshot = '', receivedSnapshotAt = 0;
let sourceKind = 'none', lastReport;
// A preset chooses UI guidance only; it is not evidence of the connected device.
const presetModel = new URLSearchParams((globalThis.location?.hash || '').slice(1)).get('model');
$('central-model').value = 'tps43';
function render() {
  if (new TextEncoder().encode($('log').value).length > MAX_LOG_BYTES) {
    lastReport = undefined;
    $('cards').replaceChildren(); $('peers').replaceChildren(); $('warnings').textContent = '';
    $('security-events').replaceChildren(); $('security-summary').textContent = '記録が大きすぎるため、読み取れませんでした。';
    $('role').textContent = ''; $('scope').textContent = ''; $('side-facts').replaceChildren();
    $('next-action').textContent = 'もう少し短い記録で試してください。上限は2 MBです。';
    renderPicture(analyzeLog(''));
    message('記録が大きすぎます。短い記録で試してください（上限2 MB）。'); return;
  }
  const options = { left: $('left').value, right: $('right').value };
  let report = analyzeLog($('log').value, options);
  if (reading && report.snapshotKey && report.snapshotKey !== lastSnapshot) {
    lastSnapshot = report.snapshotKey; receivedSnapshotAt = Date.now();
  }
  if (reading && Date.now() - receivedSnapshotAt > 15000) report = analyzeLog($('log').value, { ...options, liveStale: true });
  lastReport = report;
  renderPicture(report);
  $('role').textContent = `ログ上の役割: ${report.role} · ${report.lineCount}行`;
  $('scope').textContent = '読み取った記録から分かる範囲を表示しています。実際にキーや操作部分が動くかも確認してください。';
  $('next-action').textContent = nextAction(report, Boolean($('log').value.trim()));
  $('cards').replaceChildren(...plainCards(report).map(card => {
    const div = node('article', '', `card ${card.state}`);
    div.append(node('span', card.label, 'badge'), node('h3', card.title), node('p', card.detail));
    const details = node('details', ''); details.append(node('summary', '詳しい情報'), node('p', `${card.technical.title} — ${card.technical.detail}`), node('pre', card.evidence.map(e => `L${e.line}: ${e.text}`).join('\n'))); div.append(details);
    return div;
  }));
  $('side-facts').replaceChildren(...report.cards.slice(3).map(card => node('p', `${card.title} — ${card.detail}`)));
  $('peers').replaceChildren(...report.peers.map(peer => {
    const row = node('tr', ''); row.append(...[peer.peer, peer.localRole, peer.connected ? '接続状態 (state=2)' : '接続完了ではない', `L${peer.security}`].map(v => node('td', v))); return row;
  }));
  if (!report.peers.length) { const row = node('tr', ''); const cell = node('td', '完全な接続一覧はありません'); cell.colSpan = 4; row.append(cell); $('peers').append(row); }
  $('warnings').textContent = report.warnings.map(e => `L${e.line}: ${e.text}`).join('\n') || '該当する記録なし（正常を保証しません）';
  $('security-summary').textContent = report.security.incidents.length
    ? '以前の失敗も含まれます。今使えているなら、過去の記録だけを見て設定を消す必要はありません。'
    : report.security.summary.includes('混在') ? '複数の機器の記録が混ざっています。1台ずつ読み取ってください。'
      : '理由が分かる記録は、まだありません。「問題がない」と分かったわけではありません。';
  $('security-events').replaceChildren(...report.security.incidents.map(event => {
    const plain = plainSecurity(event);
    const div = node('article', '', `card ${event.laterEncryption || ['normal','timeout'].includes(event.kind) ? 'warn' : 'bad'}`);
    div.append(node('span', '記録から分かったこと', 'badge'), node('h3', plain.title), node('p', plain.target), node('p', plain.detail));
    if (plain.later) div.append(node('p', plain.later));
    const details = node('details', ''); details.append(node('summary', '詳しい情報'), node('p', `${event.codeLabel}: ${event.title} — ${event.detail}`), node('pre', [event.evidence, event.laterEncryption].filter(Boolean).map(e => `L${e.line}: ${e.text}`).join('\n'))); div.append(details);
    return div;
  }));
}
function renderPicture(report) {
  const picture = connectionPicture(report);
  for (const key of ['host', 'keyboards']) {
    $(`picture-${key}`).dataset.state = picture[key].state;
    $(`picture-${key}-link`).dataset.state = picture[key].state;
    $(`picture-${key}-status`).textContent = picture[key].text;
  }
  $('picture-sensor-status').textContent = picture.sensor.text;
  $('picture-sensor-status').dataset.state = picture.sensor.state;
  $('picture-model').textContent = ({tps43:'TPS43'})[$('central-model').value] || 'TPS43';
}
function renderRecovery() {
  const guide = plainRecovery($('central-model').value, $('recovery-target').value);
  $('recovery-content').hidden = !guide.steps.length;
  $('recovery-warning').textContent = guide.warning;
  $('recovery-steps').replaceChildren(...guide.steps.map(step => node('li', step)));
  $('recovery-firmware-wrap').hidden = !guide.firmware.length;
  $('recovery-firmware').replaceChildren(...guide.firmware.map((cells, i) => { const tr = node('tr', ''); tr.append(...[['まとめ役の機器', '左のキーボード', '右のキーボード'][i], ...cells.slice(1)].map(v => node('td', v))); return tr; }));
  $('recovery-note').textContent = guide.note || '';
  $('recovery-advanced').hidden = !guide.steps.length;
  $('recovery-technical-warning').textContent = guide.technical.warning;
  $('recovery-technical-steps').replaceChildren(...guide.technical.steps.map(step => node('li', step)));
}
for (const id of ['central-model', 'recovery-target']) $(id).addEventListener('change', () => {
  $('recovery-advanced').open = false; renderRecovery(); if (lastReport) renderPicture(lastReport);
});
for (const [button, panel] of [['show-help', 'recovery'], ['show-support', 'support-panel']]) $(button).onclick = () => {
  $(panel).open = true; $(panel).scrollIntoView({block:'start',behavior:'auto'}); $(panel).querySelector('summary').focus();
};
function message(text) { $('message').textContent = text; }
function setInput(text, name, kind = 'saved') {
  if (new TextEncoder().encode(text).length > MAX_LOG_BYTES) { message('記録が大きすぎます。短い記録で試してください（上限2 MB）。'); return; }
  $('log').value = text; $('source').textContent = name; sourceKind = kind; lastSnapshot = ''; receivedSnapshotAt = 0; render();
}
function controls(active) {
  for (const id of ['file', 'demo', 'auth-demo', 'serial', 'clear']) $(id).disabled = active;
  $('log').readOnly = active; $('stop').disabled = !reading;
}
$('file').addEventListener('change', async e => {
  const file = e.target.files[0]; if (!file) return;
  if (file.size > MAX_LOG_BYTES) { message('記録が大きすぎます。短い記録で試してください（上限2 MB）。'); return; }
  try { setInput(await file.text(), `保存した記録: ${file.name}（過去の状態です）`); message('読み取りました。下の結果を確認してください。'); } catch { message('ファイルを開けませんでした。もう一度選んでください。'); }
});
$('analyze').onclick = render;
$('log').addEventListener('input', () => {
  sourceKind = $('log').value.trim() ? sourceKind === 'sample' ? 'sample' : 'saved' : 'none';
  $('source').textContent = sourceKind === 'sample' ? '編集した見本・実機の状態ではありません' : '貼り付け・編集した記録';
});
for (const id of ['left','right']) $(id).addEventListener('input', render);
$('clear').onclick = () => { setInput('', 'まだ読み取っていません', 'none'); message('画面の記録だけを消しました。機器の設定は変わっていません。'); };
$('demo').onclick = () => {
  if ($('central-model').value === 'tps43') {
    setInput(`[00:00:00.400,000] <inf> iqs5xx: IQS5xx trackpad initialized
[00:00:10.000,000] <inf> cornix_ble_link: LINK detail peer=AA:BB:CC:DD:EE:02 (random) local_role=central state=2 security=L2
[00:00:10.000,100] <inf> cornix_ble_link: LINK detail peer=AA:BB:CC:DD:EE:03 (random) local_role=central state=2 security=L2
[00:00:10.000,200] <inf> cornix_ble_link: LINK status role=central connections=2 local_central=2 local_peripheral=0`, 'TPS43のお試しの見本・実機の状態ではありません', 'sample');
    message('TPS43の見本です。タッチ面の初期化と左右2台分の無線接続記録の例です。実際の入力・PC接続は未確認です。');
    return;
  }
  message('TPS43を選択してから見本を開いてください。');
};
$('save').onclick = () => {
  const url = URL.createObjectURL(new Blob([$('log').value], {type:'text/plain;charset=utf-8'}));
  const a = node('a', ''); a.href = url; a.download = 'cornix-diagnostic.log'; a.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
  message('記録を保存しました。機器を識別する番号が含まれるため、相談する相手にだけ渡してください。');
};
$('auth-demo').onclick = () => {
  setInput(`[00:00:10.000,000] <inf> cornix_ble_link: LINK detail peer=AA:BB:CC:DD:EE:02 (random) local_role=central state=2 security=L1
[00:00:10.001,000] <inf> cornix_ble_link: LINK status role=central connections=1 local_central=1 local_peripheral=0
[00:00:11.000,000] <err> zmk: Security failed: AA:BB:CC:DD:EE:02 (random) level 1 err 2
[00:00:11.001,000] <inf> cornix_ble_link: LINK disconnected peer=AA:BB:CC:DD:EE:02 (random) reason=0x06
[00:00:15.000,000] <inf> cornix_ble_link: LINK status role=central connections=0 local_central=0 local_peripheral=0`, 'つながらない例・実機の状態ではありません', 'sample');
  message('お試しの結果です。機器どうしの接続の登録が合わず、つながらなかった可能性がある例です。');
};
$('serial').onclick = async () => {
  if (!navigator.serial || !window.isSecureContext) { message('このブラウザーではUSBから読み取れません。ChromeまたはEdgeでこのページを開いてください。保存した記録でも調べられます。'); return; }
  if (opening || reading) return;
  opening = true; controls(true);
  let timer;
  try {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    // DTR enables the CDC log stream. Never switch baud or write to the port.
    await port.setSignals({ dataTerminalReady: true, requestToSend: false });
    setInput('', 'USBから読み取り中', 'usb');
    reading = true; controls(true); reader = port.readable.getReader();
    const decoder = new TextDecoder(); timer = setInterval(render, 1000);
    message('15秒ほどお待ちください。読み取れないときは「うまく読み取れないとき」を開いてください。');
    while (reading) {
      const { value, done } = await reader.read(); if (done) break;
      let content = $('log').value + decoder.decode(value, {stream:true});
      if (content.length > MAX_LOG_BYTES / 2) { content = content.slice(-MAX_LOG_BYTES / 2); content = content.slice(content.indexOf('\n') + 1); }
      $('log').value = content;
    }
  } catch (err) { message(err.name === 'NotFoundError' ? '機器が選ばれませんでした。「USBで調べる」からやり直せます。' : 'USBから読み取れませんでした。ケーブルと電源を確認し、記録を読んでいる別のアプリがあれば止めてから、もう一度試してください。'); }
  finally {
    clearInterval(timer); reading = false; opening = false;
    try { reader?.releaseLock(); } catch {} reader = undefined;
    try { await port?.close(); } catch {} port = undefined;
    $('source').textContent = '読み取り終了（終了時までの記録です）'; controls(false); render();
  }
};
$('stop').onclick = async () => { reading = false; await reader?.cancel(); message('読み取りを止めました。機器の設定は変わっていません。'); };
$('make-summary').onclick = () => {
  render();
  if (!lastReport) { $('support-message').textContent = '記録が大きすぎるため、相談文を作れませんでした。短い記録で試してください。'; return; }
  $('support-text').value = supportSummary(lastReport, {
    central:$('central-model').value, target:$('recovery-target').value, symptom:$('symptom').value,
    tried:['power', 'pairing', 'reset'].filter(key => $(`tried-${key}`).checked), source:sourceKind,
  });
  $('support-output').hidden = false;
  $('support-message').textContent = '相談文を作りました。内容を確認してからコピーし、製作者へのメッセージに貼り付けてください。';
};
$('copy-summary').onclick = async () => {
  try { await navigator.clipboard.writeText($('support-text').value); $('support-message').textContent = 'コピーしました。製作者へのメッセージに貼り付けて送ってください。'; }
  catch { $('support-text').focus(); $('support-text').select(); $('support-message').textContent = '自動でコピーできませんでした。文章を選択したので、右クリックしてコピーしてください。'; }
};
$('save-summary').onclick = () => {
  const url = URL.createObjectURL(new Blob([$('support-text').value], {type:'text/plain;charset=utf-8'}));
  const link = node('a', ''); link.href = url; link.download = 'cornix-support.txt'; link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
  $('support-message').textContent = '相談文を保存しました。このファイルを製作者に送れます。';
};
render();
renderRecovery();
