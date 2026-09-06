import { recoveryGuide } from './security.mjs';

// The parser keeps precise technical evidence. This layer supplies everyday-language UI copy.
export function plainCards(report) {
  const [split, host, sensor] = report.cards;
  const count = split.title.match(/(\d+)\/(\d+)/);
  return [
    { ...split, label:report.role === 'peripheral' ? 'まとめ役の機器' : '左右のキーボード',
      title:count ? `${count[2]}台のうち${count[1]}台分の無線接続を確認` : 'まだ確認できません',
      detail:count && count[1] === count[2]
        ? '無線ではつながっています。実際に左右のキーを押して、文字が入力されるか確認してください。'
        : count ? '必要な接続がそろっていません。左右のキーボードと、まとめ役の機器の電源を確認してください。'
          : '記録が足りないか、接続の状態が変わっています。電源を確認し、もう15秒ほど読み取ってください。', technical:split },
    { ...host, label:'パソコン',
      title:host.state === 'good' ? 'Bluetoothの接続記録があります' : host.state === 'warn' ? 'Bluetoothの接続を確認できません' : 'まだ確認できません',
      detail:host.state === 'good' ? '記録を読み取った時点ではつながっていました。文字入力ができるかも確認してください。'
        : host.state === 'warn' ? 'パソコンのBluetooth設定を確認してください。USBケーブルでの接続は、この項目では判定しません。'
          : 'パソコンへの接続を判断できる記録がありません。まとめ役の機器をUSBケーブルでつないで読み取ってください。', technical:host },
    { ...sensor, label:'ボール・スティック・タッチ面',
      title:sensor.state === 'bad' ? '操作部分に問題が記録されています' : sensor.state === 'warn' ? '準備ができた記録があります' : 'まだ確認できません',
      detail:sensor.state === 'bad' ? '操作部分をうまく読み取れていない可能性があります。接続の登録を消す前に、機器に合った設定か、配線に問題がないかを確認してもらってください。'
        : sensor.state === 'warn' ? '実際に動かして、画面の矢印が動くか確認してください。準備できた記録だけでは、動作までは分かりません。'
          : '電源を入れた直後の記録がないと、ここは確認できないことがあります。「故障」という意味ではありません。', technical:sensor },
  ];
}

export function nextAction(report, hasLog) {
  if (!hasLog) return 'まず上の「USBで調べる」を押してください。記録を読み取るだけで、設定は消えません。';
  if (report.security.incidents.some(e => e.resetCandidate)) return '接続相手の確認に失敗した記録があります。下の「設定を消して、最初からつなぎ直す」から手順を確認できます。';
  if (report.cards[2].state === 'bad') return '操作部分のエラーが見つかりました。設定を消す前に、この結果を製作者やサポートへ見せてください。';
  if (report.cards[0].state === 'bad') return 'まず左右のキーボードと、まとめ役の機器の電源を確認してください。近くに置いて、もう15秒ほど待ってみましょう。';
  if (report.cards[0].state === 'unknown') return 'まだ判断できません。まとめ役の機器をつないで、15秒以上読み取ってください。記録が出ないときは「うまく読み取れないとき」を開いてください。';
  if (report.cards[1].state === 'warn') return 'パソコンのBluetooth設定を確認してください。USBケーブルで使いたい場合は、まず文字入力を試してください。';
  return '左右のキーと、ボールやタッチ面を動かしてみてください。実際に入力できれば確認完了です。';
}

export function connectionPicture(report) {
  const [split, host, sensor] = report.cards;
  const count = split.title.match(/(\d+)\/(\d+)/);
  return {
    host:{state:host.state, text:host.state === 'good' ? '✓ 接続の記録あり' : host.state === 'warn' ? '！接続を確認できません' : '？ まだ確認できません'},
    keyboards:report.role === 'peripheral' || !count
      ? {state:'unknown',text:'？ 左右の接続はまだ分かりません'}
      : {state:split.state,text:`${split.state === 'bad' ? '！' : '△'} ${count[2]}台のうち${count[1]}台分の接続記録`},
    sensor:{state:sensor.state,text:sensor.state === 'bad' ? '！操作部分にエラーの記録' : sensor.state === 'warn' ? '△ 操作部分の準備はできています' : '？ 操作部分はまだ確認できません'},
  };
}

export function plainSecurity(event) {
  const messages = {
    auth:['接続相手の確認に失敗しました', '以前の接続の登録が合わなくなっている可能性があります。ほかの原因もあるため、まず下の案内で接続相手を確認してください。'],
    key:['接続に必要な登録が見つかりません', '片方だけ接続の登録が消えている可能性があります。つなぎ直す手順を、下の案内で確認してください。'],
    timeout:['接続相手から返事がありませんでした', '電源が切れている、距離が離れている、といった原因が考えられます。まず電源と距離を確認してください。設定を消す必要があるとは限りません。'],
    normal:['接続が切れた記録があります', '電源を切ったときなどにも残る記録です。この記録だけでは故障や登録の問題とは分かりません。'],
    policy:['接続が許可されませんでした', '相手の機器の設定や、登録できる条件が合っていない可能性があります。設定を消すだけでは直らない場合があります。'],
    profile:['別の相手が登録されています', '使いたいパソコンの登録先を選んでください。操作が分からない場合は、下の「設定を消して、最初からつなぎ直す」から手順を確認できます。'],
    unknown:['接続がうまくいかなかった記録があります', 'この記録だけでは原因は分かりません。設定を消さず、記録を保存して詳しい人に見てもらってください。'],
  };
  const [title, detail] = messages[event.kind] || messages.unknown;
  return {title, detail, target:{host:'パソコンとの接続',split:'キーボードとまとめ役の機器との接続',unknown:'どの機器との接続かは、まだ分かりません'}[event.target],
    later:event.laterEncryption ? 'その後、同じ相手と接続できた記録があります。今使えているなら、設定は消さずにそのままお使いください。' : ''};
}

export function plainRecovery(central, target) {
  const selected = recoveryGuide(central, target);
  if (!selected.steps.length) return {...selected, warning:'お使いの機器と、つながらない相手を選んでください。機種が分からないときは、設定を消さずに製作者へ確認してください。', technical:selected};
  // One full-reset procedure for both symptoms; the user's reported target stays unchanged.
  const technical = recoveryGuide(central, 'split');
  return {...technical,
    warning:'通常の書き換えとは違い、この手順では3台の接続登録と保存したキー設定などを消して、最初からつなぎ直します。',
    steps:[
      '3台それぞれに使う「設定を消すファイル」と「元に戻すファイル」を用意します。下の一覧で機器とファイル名を照合してください。ファイルを持っていない、機種が分からない場合は、ここで止めて確認してください。',
      '3台をそれぞれ、書き込みを受け付ける状態にします。通常はリセットボタンを素早く2回押します。パソコンに新しいUSBドライブが表示されない場合は、無理に進めず機器の説明書を確認してください。',
      '1台ずつ、その機器に対応する「設定を消すファイル」をUSBドライブへコピーします。コピー後に機器が起動し、設定を消す処理が終わるのを待ちます。3台とも終わるまで「元に戻すファイル」は入れないでください。',
      '3台とも設定を消したら、もう一度書き込みを受け付ける状態にします。今度は各機器に対応する「元に戻すファイル」をコピーします。この作業をしないと、機器は使えません。',
      '3台の電源をほぼ同時に入れ直し、自動でつながるのを待ちます。パソコンに残っている古いBluetoothの登録を削除し、機器をもう一度登録します。',
      'この画面で15秒以上読み取り、左右のキーと操作部分も実際に試します。直らなければ何度も設定を消さず、記録を保存して相談してください。',
    ], technical};
}

export function supportSummary(report, {central = '', target = '', symptom = '', tried = [], source = 'none'} = {}) {
  const models = {tps43:'TPS43（タッチ面）'};
  const targets = {host:'まとめ役の機器とパソコン', split:'左右のキーボードとまとめ役の機器'};
  const sources = {none:'記録はまだ読み取っていません', saved:'保存した記録・貼り付けた記録（過去の状態）', usb:'USBから読み取った記録（読み取った範囲のみ）', sample:'お試しの見本です。実機の診断ではありません'};
  const actions = {power:'電源を入れ直した', pairing:'パソコンと機器の接続を登録し直した', reset:'機器の設定を消し、元のファイルを書き込み直した'};
  const redact = text => text.replace(/\b[0-9a-f]{2}(?:[:-][0-9a-f]{2}){5}\b/gi, '［機器の番号は省略］');
  const facts = plainCards(report).map(c => `・${c.label}: ${c.title}`);
  const seen = new Set();
  const incidents = report.security.incidents.map(e => {
    const plain = plainSecurity(e);
    return `・${plain.target}: ${plain.title}${plain.later ? '（その後、同じ相手と接続できた記録あり）' : ''}`;
  }).filter(text => !seen.has(text) && seen.add(text));
  return redact([
    '接続についての相談', '',
    `使っている機器（本人が選択）: ${models[central] || '分からない・未選択'}`,
    `つながらない相手（本人が選択）: ${targets[target] || '分からない・未選択'}`,
    `困っていること: ${symptom.trim().slice(0, 1000) || '未記入'}`, '',
    '試したこと（本人の回答）:',
    ...(tried.filter(key => Object.hasOwn(actions, key)).length ? tried.filter(key => Object.hasOwn(actions, key)).map(key => `・${actions[key]}`) : ['・未回答（何もしていない、と判断したわけではありません）']), '',
    `調べ方: ${sources[source] || sources.none}`, '',
    '画面の診断結果:', ...facts, '',
    'つながらなかった理由の手がかり:',
    ...(incidents.length ? incidents : ['・理由が分かる記録はありません。問題がないと確認できたわけではありません。']), '',
    '次に確認すること:', nextAction(report, source !== 'none'), '',
    '※自動診断は記録から分かる範囲です。現在の接続・実際の文字入力・操作部分の動作は未確認です。',
    '※この相談文を作る操作では、機器の設定を変えたり消したりしていません。',
    '※元の記録や機器の接続番号は添付していません。',
  ].join('\n'));
}
