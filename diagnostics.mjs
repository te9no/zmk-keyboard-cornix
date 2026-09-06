// Parse observations, not assumptions: a BLE ACL link is not a ready ZMK split service.
import { createSecurityObserver } from './security.mjs';
export const MAX_LOG_BYTES = 2 * 1024 * 1024;
const MAC = /[0-9a-f]{2}(?::[0-9a-f]{2}){5}/i;
const ANSI = /\x1b\[[0-?]*[ -/]*[@-~]/g;
const fact = (state, title, detail, evidence = []) => ({ state, title, detail, evidence });
const evidence = (line, text) => ({ line, text });

export function analyzeLog(raw, { left = '', right = '', liveStale = false } = {}) {
  const text = raw.replace(ANSI, '').replace(/\r/g, '');
  const lines = text.split('\n');
  const ports = new Set([...text.matchAll(/\b(COM\d+)\s*:/g)].map(m => m[1]));
  let role, snapshot, pending = [], profiles = new Map(), endpoint, sensor;
  let sensors = new Map();
  let security = createSecurityObserver();
  let lastTime = null, latestTime = null, boots = 0, identity, changeAfterSnapshot = false;
  let warnings = [];
  const reset = () => {
    role = snapshot = endpoint = sensor = identity = undefined;
    pending = []; profiles = new Map(); sensors = new Map(); warnings = []; changeAfterSnapshot = false;
    security = createSecurityObserver();
    lastTime = latestTime = null; boots++;
  };
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const source = evidence(i + 1, line);
    const ts = line.match(/\[(\d+):(\d{2}):(\d{2})[.,](\d{3})(?:,\d+)?\]/);
    const time = ts ? (+ts[1] * 3600 + +ts[2] * 60 + +ts[3]) * 1000 + +ts[4] : null;
    // Initial sensor logs often precede the boot banner: retain those logs.
    if (time !== null && lastTime !== null && time < lastTime - 1000) reset();
    if (/\*\*\* Booting Zephyr/.test(line) && lastTime !== null && lastTime > 2000) reset();
    if (time !== null) lastTime = latestTime = time;
    security.observe(line, source);
    let m;
    if ((m = line.match(/LINK identity id=\d+ addr=(.+)/))) identity = m[1];
    if ((m = line.match(/LINK detail peer=(.+?) local_role=(central|peripheral) state=(\d+) security=L(\d+)/))) {
      pending.push({ peer: m[1], localRole: m[2], connected: +m[3] === 2, security: +m[4], evidence: source });
    }
    if ((m = line.match(/LINK status role=(central|peripheral) connections=(\d+) local_central=(\d+) local_peripheral=(\d+)\s*$/))) {
      role = m[1];
      const peers = [...new Map(pending.map(p => [p.peer, p])).values()];
      const complete = peers.length === +m[2] &&
        peers.filter(p => p.localRole === 'central').length === +m[3] &&
        peers.filter(p => p.localRole === 'peripheral').length === +m[4];
      snapshot = { role, peers, complete, time, evidence: source };
      pending = []; profiles = new Map(); changeAfterSnapshot = false;
    } else if (/LINK status/.test(line)) {
      // A damaged newer record must not leave an older snapshot looking current.
      if (snapshot) snapshot.complete = false;
      pending = []; profiles = new Map();
    }
    if ((m = line.match(/LINK host_profile index=(\d+) active=([01]) open=([01]) connected=([01]) peer=(.+)/))) {
      profiles.set(+m[1], { index: +m[1], active: m[2] === '1', open: m[3] === '1', connected: m[4] === '1', peer: m[5], evidence: source });
    }
    if (/LINK (?:connected|disconnected) peer=/.test(line) && snapshot) changeAfterSnapshot = true;
    if ((m = line.match(/Endpoint changed: (USB|BLE)/))) endpoint = { name: m[1], evidence: source };
    const sensorMatch = line.match(/(?:iqs9151|iqs5xx|ads1220|analog_axis_hires|pmw3610|PMW_DIAG)/i);
    const sensorLine = Boolean(sensorMatch);
    let observation;
    if (sensorLine && /<err>|unexpected product|RDY timeout|async_ready=0.*init_err=-[1-9]/i.test(line)) {
      observation = fact('bad', 'センサーにエラー記録', '配線・給電・モジュールに合うファームを確認してください。自動リセットは行いません。', [source]);
    } else if (sensorLine && /Initiali[sz]ed|async_ready=1.*init_err=0|initialization (?:complete|successful)/i.test(line)) {
      // A later explicit successful initialization can supersede an earlier error.
      observation = fact('warn', '初期化の成功記録あり', '初期化ログだけでは、指・ボール・キーの入力が届くことまでは確認できません。', [source]);
    }
    if (observation) sensors.set(sensorMatch[0].toLowerCase().replace('pmw_diag', 'pmw3610'), observation);
    if (/<err>|LINK disconnected|LINK connected.*err=0x(?!00)[0-9a-f]{2}|RDY timeout/i.test(line)) warnings.push(source);
  }
  sensor = [...sensors.values()].find(s => s.state === 'bad') || [...sensors.values()].at(-1);
  const unknown = () => fact('unknown', '判定材料なし', 'Centralのログ用CDCを115200 baudで15秒以上読み取ってください。Studio用ポートとは異なります。');
  let split = unknown(), host = unknown();
  let peers = [], sideFacts = [];
  const stale = liveStale || (snapshot?.time != null && latestTime - snapshot.time > 15000);
  const valid = snapshot?.complete && !pending.length && !stale && !changeAfterSnapshot && ports.size <= 1;
  if (snapshot) {
    if (!snapshot.complete || pending.length) split = fact('unknown', '接続一覧が不完全', 'LINK statusとLINK detailがそろっていません。接続数だけでは正常と判定しません。', [snapshot.evidence]);
    else if (stale) split = fact('unknown', '接続情報が古い', '15秒以上、新しい接続スナップショットを確認できていません。', [snapshot.evidence]);
    else if (changeAfterSnapshot) split = fact('unknown', '接続が変化・再確認待ち', 'スナップショットの後に接続／切断イベントがあります。次のLINK statusを待ってください。', [snapshot.evidence]);
    else {
      peers = snapshot.peers;
      const remoteRole = role === 'central' ? 'central' : 'peripheral';
      const connected = peers.filter(p => p.localRole === remoteRole && p.connected);
      const expected = role === 'central' ? 2 : 1;
      split = fact(connected.length === expected ? 'warn' : 'bad', `${role === 'central' ? 'Peripheral側' : 'Central側'} BLEリンク ${connected.length}/${expected}`, connected.length === expected
        ? '無線リンクは確認できました。Splitサービスの準備・左右の実キー入力は別途確認が必要です。'
        : '必要な無線リンクがそろっていません。相手の電源・ファームの役割・bondの組み合わせを確認してください。', [snapshot.evidence, ...connected.map(p => p.evidence)]);
    }
  }
  const active = [...profiles.values()].find(p => p.active);
  if (valid && active) {
    host = fact(active.connected ? 'good' : 'warn', `PC向けBLE Profile ${active.index}: ${active.connected ? '接続記録あり' : active.open ? '未登録' : '未接続'}`, active.connected
      ? 'PC向けBLEプロファイルの接続です。左右Peripheralの接続とは別です。'
      : active.open ? '空きプロファイルです。広告中かどうかは、この項目だけでは断定できません。' : '登録情報はありますが、この観測時点では接続されていません。', [active.evidence]);
  } else if (role === 'peripheral' && valid) host = fact('unknown', 'Peripheral側ログでは不明', 'PCへの接続状態はCentralのログで確認してください。');
  if (endpoint) host.detail += ` 最後の出力先変更記録: ${endpoint.name}（現在の接続保証ではありません）。`;
  if (ports.size > 1) {
    split = host = fact('unknown', '複数COMポートのログが混在', '1デバイス・1ポートずつ診断してください。混在ログから状態を合成しません。');
    sensor = undefined; peers = [];
  }
  for (const [side, address] of [['左', left], ['右', right]]) {
    const mac = address.trim().toUpperCase();
    if (!mac) continue;
    if (!new RegExp(`^${MAC.source}$`, 'i').test(mac)) {
      sideFacts.push(fact('unknown', `${side}: アドレスの書式エラー`, 'AA:BB:CC:DD:EE:FF の形式で入力してください。')); continue;
    }
    const found = peers.find(p => p.localRole === 'central' && p.peer.toUpperCase().startsWith(mac));
    sideFacts.push(!valid || role !== 'central' ? fact('unknown', `${side}: 判定不可`, '新しい完全なCentral側スナップショットが必要です。')
      : fact(found?.connected ? 'warn' : 'bad', `${side}: ${found?.connected ? 'BLEリンクあり' : 'BLEリンク未確認'}`, '入力したアドレスとの照合です。左右は接続順やBLE roleから自動推定しません。', found ? [found.evidence] : [snapshot.evidence]));
  }
  return {
    role: role || 'unknown', identity, boots, lineCount: lines.length, snapshotKey: snapshot ? `${snapshot.time}:${snapshot.evidence.line}` : null,
    cards: [split, host, sensor || fact('unknown', 'センサー判定材料なし', '起動直後のログが必要です。エラーがないことだけでは正常と判定しません。'), ...sideFacts],
    peers: valid ? peers : [], warnings: warnings.slice(-12),
    security: security.report(role, ports.size > 1),
    scope: 'ログ末尾の観測を表示します。実機の現在状態・打鍵の到達・HID通知の成功を保証するものではありません。',
  };
}
