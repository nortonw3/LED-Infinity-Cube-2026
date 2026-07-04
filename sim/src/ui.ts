import { getState, selectAnimation, setBrightness, setDemo, getAnimNames, type Mode } from './engine';
import { startPaletteFade, PALETTE_NAMES, getPaletteIndex } from './palettes';
import { startDemoTrack, startSystemAudio, startSynthetic, stopSource, getSourceKind } from './audio/sources';

const MODES: Mode[] = ['static', 'audio', 'voice'];

export function initUI(): void {
  const ui = document.createElement('div');
  ui.id = 'ui';
  ui.innerHTML = `
    <strong>LED Infinity Cube</strong>
    <label>Mode</label><select id="mode"></select>
    <label>Animation</label><select id="anim"></select>
    <label>Palette</label><select id="pal"></select>
    <label>Brightness</label><input id="bright" type="range" min="5" max="255" step="1">
    <div class="row">
      <button id="demoReel">Auto-cycle</button>
    </div>
    <label>Audio source</label>
    <div class="row">
      <button id="srcDemo">Demo track</button>
      <button id="srcSystem">System audio</button>
    </div>
    <div class="row">
      <button id="srcSynth">Synthetic</button>
      <button id="srcNone">Silent</button>
    </div>
    <div class="err" id="err"></div>
  `;
  document.body.appendChild(ui);

  const modeSel = ui.querySelector<HTMLSelectElement>('#mode')!;
  const animSel = ui.querySelector<HTMLSelectElement>('#anim')!;
  const palSel = ui.querySelector<HTMLSelectElement>('#pal')!;
  const bright = ui.querySelector<HTMLInputElement>('#bright')!;
  const demoBtn = ui.querySelector<HTMLButtonElement>('#demoReel')!;
  const err = ui.querySelector<HTMLDivElement>('#err')!;

  MODES.forEach((m) => modeSel.add(new Option(m.toUpperCase(), m)));
  PALETTE_NAMES.forEach((n, i) => palSel.add(new Option(n, String(i))));

  function fillAnims(mode: Mode): void {
    animSel.innerHTML = '';
    getAnimNames(mode).forEach((n, i) => animSel.add(new Option(n, String(i))));
  }

  function refresh(): void {
    const s = getState();
    modeSel.value = s.mode;
    if (animSel.options.length !== getAnimNames(s.mode).length ||
        animSel.options[0]?.text !== getAnimNames(s.mode)[0]) fillAnims(s.mode);
    animSel.value = String(s.index);
    palSel.value = String(getPaletteIndex());
    bright.value = String(s.brightness);
    demoBtn.classList.toggle('active', s.demo);
    ui.classList.toggle('faded', s.demo);
    const kind = getSourceKind();
    ui.querySelector('#srcDemo')!.classList.toggle('active', kind === 'demo');
    ui.querySelector('#srcSystem')!.classList.toggle('active', kind === 'system');
    ui.querySelector('#srcSynth')!.classList.toggle('active', kind === 'synthetic');
    ui.querySelector('#srcNone')!.classList.toggle('active', kind === 'none');
  }

  modeSel.onchange = () => { setDemo(false); fillAnims(modeSel.value as Mode); selectAnimation(modeSel.value as Mode, 0); refresh(); };
  animSel.onchange = () => { setDemo(false); selectAnimation(modeSel.value as Mode, Number(animSel.value)); refresh(); };
  palSel.onchange = () => { startPaletteFade(Number(palSel.value)); };
  bright.oninput = () => { setBrightness(Number(bright.value)); };
  demoBtn.onclick = () => { setDemo(!getState().demo); refresh(); };

  ui.querySelector<HTMLButtonElement>('#srcDemo')!.onclick = async () => {
    err.textContent = '';
    try { await startDemoTrack(); } catch (e) { err.textContent = String((e as Error).message); }
    refresh();
  };
  ui.querySelector<HTMLButtonElement>('#srcSystem')!.onclick = async () => {
    err.textContent = '';
    try { await startSystemAudio(); } catch (e) { err.textContent = String((e as Error).message); }
    refresh();
  };
  ui.querySelector<HTMLButtonElement>('#srcSynth')!.onclick = () => { startSynthetic(); refresh(); };
  ui.querySelector<HTMLButtonElement>('#srcNone')!.onclick = () => { stopSource(); refresh(); };

  setInterval(refresh, 500);   // pick up demo-reel changes
  refresh();
}
