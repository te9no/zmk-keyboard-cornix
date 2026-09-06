// Numeric namespaces MUST stay separate: HCI 0x06 is key missing,
// bt_security_err 6 is pairing not allowed. Values verified against the pinned Zephyr headers.
const hciReasons = new Map([
  [0x05, ['auth', '認証失敗', 'bond不整合や認証条件の不一致が候補です。鍵破損とまでは断定できません。']],
  [0x06, ['key', 'PIN／暗号鍵がない', '片側の登録消去・古いbondの残存が候補です。対象を確認して再ペアリングを検討してください。']],
  [0x08, ['timeout', '接続タイムアウト', '電源・距離・電波状況を先に確認してください。このコードだけでbond不整合とは判定しません。']],
  [0x13, ['normal', '相手側から切断', '通常の切断でも発生します。認証失敗を示すコードではありません。']],
  [0x16, ['normal', '自分側から切断', '通常の切断でも発生します。認証失敗を示すコードではありません。']],
  [0x18, ['policy', 'ペアリング不許可', '登録済みプロファイルや相手側の許可状態を確認してください。鍵不足とは別です。']],
  [0x2f, ['policy', '要求セキュリティを満たせない', '相手の認証要件・設定を確認してください。Resetだけで直るとは限りません。']],
  [0x3c, ['timeout', 'Directed advertisingタイムアウト', '相手の起動・電源・距離を確認してください。認証失敗とは断定しません。']],
  [0x3e, ['timeout', '接続確立に失敗', '相手の電源・距離・電波状況を先に確認してください。bond不整合とは断定しません。']],
]);
const securityReasons = new Map([
  [1, hciReasons.get(0x05)], [2, hciReasons.get(0x06)],
  [3, ['policy', 'OOB認証データがない', '認証方式の設定を確認してください。']],
  [4, hciReasons.get(0x2f)],
  [5, ['policy', 'ペアリング非対応', '相手側の対応・認証設定を確認してください。']],
  [6, hciReasons.get(0x18)],
  [7, ['policy', '認証パラメーターが不正', 'ファーム・相手側の認証条件を確認してください。']],
  [8, ['auth', '配布鍵が拒否された', '相手の認証条件や保存済みbondを確認してください。']],
  [9, ['unknown', '認証手続き失敗・詳細不明', '具体的な原因はこの記録から特定できません。']],
]);
const address = value => value?.trim().toUpperCase(); // Retain public/random type; never merge them.

export function createSecurityObserver() {
  const incidents = [], roles = new Map(), hosts = new Set(), encrypted = new Map();
  function observe(line, evidence) {
    let m;
    if ((m = line.match(/LINK detail peer=(.+?) local_role=(central|peripheral) state=(\d+) security=L(\d+)/))) {
      roles.set(address(m[1]), m[2]);
      if (+m[3] === 2 && +m[4] >= 2) encrypted.set(address(m[1]), evidence);
    }
    if ((m = line.match(/LINK host_profile index=\d+ active=[01] open=[01] connected=[01] peer=(.+)/))) hosts.add(address(m[1]));
    if ((m = line.match(/Security changed: (.+?) level (\d+)\s*$/)) && +m[2] >= 2) encrypted.set(address(m[1]), evidence);
    let code, peer, namespace, description;
    if ((m = line.match(/LINK (connected|disconnected) peer=(.+?) (?:err|reason)=0x([\da-f]{2})\s*$/i))) {
      code = parseInt(m[3], 16); peer = m[2]; namespace = 'HCI';
      if (code === 0) return;
      description = hciReasons.get(code);
    } else if ((m = line.match(/Security failed: (.+?) level \d+ err (\d+)\s*$/))) {
      code = +m[2]; peer = m[1]; namespace = 'bt_security_err';
      if (code === 0) return;
      description = securityReasons.get(code);
    } else if ((m = line.match(/Rejecting pairing request to taken profile (\d+)\s*$/))) {
      incidents.push({ kind:'profile', title:`登録済みProfile ${m[1]}へのペアリングを拒否`, detail:'正しいプロファイルを選んでください。再登録する場合はPC側と該当プロファイル側の登録を解除します。', target:'host', evidence });
      return;
    } else return;
    const [kind, title, detail] = description || ['unknown', '未対応の理由コード', 'コードを別のエラー体系として推測しません。前後のログを確認してください。'];
    incidents.push({kind, title, detail, peer, namespace, code, evidence});
  }
  function report(role, mixed = false) {
    if (mixed) return { incidents:[], summary:'複数ポートのログが混在しているため、認証対象は診断しません。' };
    const mapped = incidents.map(event => {
      const key = address(event.peer);
      const localRole = roles.get(key);
      const target = event.target || (role === 'peripheral' && key ? 'split' : role === 'central' && key
        ? localRole === 'central' ? 'split' : localRole === 'peripheral' || hosts.has(key) ? 'host' : 'unknown'
        : 'unknown');
      const later = encrypted.get(key);
      const laterEncryption = later && later.line > event.evidence.line ? later : null;
      return { ...event, target, laterEncryption,
        resetCandidate: !laterEncryption && ['auth', 'key'].includes(event.kind),
        codeLabel: event.namespace === 'HCI' ? `HCI 0x${event.code.toString(16).padStart(2, '0')}` : event.namespace ? `bt_security_err ${event.code}` : 'ZMKプロファイル',
      };
    });
    return {incidents:mapped.slice(-12), summary:mapped.length
      ? '認証・切断の履歴です。原因の候補と確認先を示しますが、現在も失敗していることやResetで直ることを保証しません。'
      : '認証失敗を特定できる記録はありません。失敗していないという意味ではありません。接続試行の前後を含むログが必要です。'};
  }
  return {observe, report};
}

// User-selected scope, never inferred from a generic connection count or timeout.
export function recoveryGuide(central, target) {
  const models = {tps43:'cornix_tps43_production'};
  if (!models[central] || !['host', 'split'].includes(target)) return {
    warning:'まず実機のCentralと、接続に失敗している相手を選んでください。選択はログによる確定診断ではありません。', steps:[], firmware:[],
  };
  if (target === 'host') return {
    warning:'PCだけの問題なら、まず該当するPCプロファイルだけを再登録します。左右Peripheralの全設定Resetは不要です。',
    steps:[
      '対象PC用のBluetoothプロファイルを確認します。空きプロファイルへ切り替えるだけで解決する場合もあります。',
      '再登録する場合、PCのBluetooth設定から対象キーボードの登録を削除し、Central側はそのPCのプロファイルだけをBT_CLR（設定済みキーや対応Studio操作）で消去します。別のPCの登録を消さないでください。',
      'PCからペアリングし直します。USB給電中にBLE入力を試す場合は出力先をBLEへ切り替え、ログと実キー入力の両方を確認します。',
      'BT_CLRを操作できない場合の選択肢: cornix_tps43_host_bond_reset.uf2をTPS43へ書き込み、起動後の「Central host profiles cleared」ログを確認します。全PC/スマートフォンのbondが消えますが、左右のbondは維持します。必ずcornix_tps43_production.uf2へ戻してから再ペアリングしてください。復旧ファームのままでは起動のたびに消去されます。',
    ], firmware:[],
  };
  return {
    warning:'全設定消去: Centralと左右3台のbond、PCプロファイル、保存済みのキーマップ等の設定が失われます。必要な設定を先に控え、通常ファームも3台分用意してください。認証要件の不一致や電波問題にはResetが効かない場合があります。',
    steps:[
      '対象がXIAO BLEのTPS43 CentralとCornix左右であること、ファームの役割と左右を確認します。別のCentralは電源を切ります。異なる基板・Flash配置ではこの一覧を使わないでください。',
      '3台をブートローダーモードにし、各デバイスへ対応するsettings_reset.uf2を書き込みます。各Resetファームを一度起動して設定消去が完了するのを待ち、全3台の消去が終わるまで通常ファームへ戻さないでください。',
      '全3台の消去後、各デバイスを再びブートローダーモードにし、下表の通常ファームへ戻します。Settings ResetのままではBluetooth接続・入力はできません。',
      'Centralと左右をほぼ同時に再起動して自動ペアリングを待ちます。PCのBluetooth設定に残った旧登録を削除し、CentralをPCへ再ペアリングします。',
      '15秒以上のログを取り直し、左右2本のBLEリンクとPCプロファイル、実キー入力を確認します。改善しなければResetを繰り返さずログを保存してください。',
    ],
    firmware:[
      ['Central（XIAO BLE）', 'cornix_tps43_settings_reset.uf2', `${models[central]}.uf2`],
      ['Cornix左（cornix_ph_left）', 'cornix_left_settings_reset.uf2', 'cornix_left_production.uf2'],
      ['Cornix右', 'cornix_right_settings_reset.uf2', 'cornix_right_production.uf2'],
    ],
    note:'Central用Resetは名前にtps43がありますが、build.yamlではxiao_ble//zmk + settings_reset + NVS設定の共通イメージです（センサードライバーなし）。実機での復旧動作は未検証です。左右のbond_resetは毎回起動時に設定全体を消すため、この全体復旧には使用しません。',
  };
}
