// Shared by every CleanLink page: formatting, funding bars, and the
// spot list + detail panel. Each page adds only its own role's forms.

// Names typed by users go into the page as text, never as HTML
const esc = v => String(v ?? '').replace(/[&<>"']/g, c =>
  ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

const rs = n => 'Rs ' + Number(n).toLocaleString('en-IN');
const SIZES = ['Small', 'Medium', 'Large'];
const WORKERS = [2, 5, 10];
const WAGE = 500;  // per worker, same as WAGE_PER_WORKER in waste_spot.h

// Floor, so a spot never shows 100% until it is really fully funded
const percent = s => s.cleanupCost > 0 ? Math.min(100, Math.floor(s.fundedAmount * 100 / s.cleanupCost)) : 0;
const isOpen = s => s.status === 'Verified';
const isFunded = s => ['Funded', 'Assigned', 'Completed'].includes(s.status);

// Google Maps opens a pin at "?q=latitude,longitude"
function mapsLink(s) {
  return `https://www.google.com/maps?q=${s.latitude},${s.longitude}`;
}

// Small bar + label for a spot in the list
function miniBar(s) {
  if (isOpen(s) || isFunded(s)) {
    const p = isFunded(s) ? 100 : percent(s);
    return `<span class="bar${p === 100 ? ' full' : ''}"><i style="width:${p}%"></i></span>
            <em class="pct">${isFunded(s) ? s.status.toLowerCase() : p + '%'}</em>`;
  }
  return `<span class="bar off"></span><em class="pct">${s.status.toLowerCase()}</em>`;
}

// Big bar in the detail panel. "from" is the width it starts at, so it can grow after a donation.
function fundBlock(s, from) {
  const p = isFunded(s) ? 100 : percent(s);
  const start = from ?? p;

  if (s.status === 'Pending') {
    return `<div class="fund">
      <div class="head"><b>Not open yet</b><span>est. ${rs(s.cleanupCost)}</span></div>
      <div class="bigbar off"></div>
      <p>Opens for donations once a moderator verifies this spot.</p></div>`;
  }
  if (s.status === 'Rejected') {
    return `<div class="fund">
      <div class="head"><b>Rejected</b><span>by moderator #${s.moderatorID}</span></div>
      <div class="bigbar off"></div>
      <p>Not taking donations.</p></div>`;
  }

  const left = s.cleanupCost - s.fundedAmount;
  const done = { Funded: 'Fully funded. Waiting for workers.', Assigned: 'Fully funded. Workers assigned.', Completed: 'Cleaned up.' };
  return `<div class="fund">
    <div class="head"><b>${rs(s.fundedAmount)}</b><span>of ${rs(s.cleanupCost)} · ${p}%</span></div>
    <div class="bigbar${p === 100 ? ' full' : ''}"><i class="fill" style="width:${start}%" data-to="${p}"></i></div>
    <p>${isFunded(s) ? done[s.status] : `${rs(left)} to go · ${s.workersNeeded} workers × ${rs(WAGE)}`}</p></div>`;
}

// Date and time from a saved Unix time (seconds)
const when = t => new Date(t * 1000).toLocaleString('en-IN', { dateStyle: 'medium', timeStyle: 'short' });

// POST JSON and return the reply, or throw with the server's error message
async function post(url, body) {
  let res;
  try {
    res = await fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  } catch {
    throw new Error('Could not reach the server. Is it running?');
  }
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || 'Something went wrong.');
  return data;
}

// Spot / city / money counts, on pages that have them (the home page)
function showStats(spots) {
  const count = document.getElementById('count');
  if (!count) return;
  count.textContent = `${spots.length} spot${spots.length === 1 ? '' : 's'}`;
  const cities = new Set(spots.map(s => s.city).filter(Boolean)).size;
  document.getElementById('cities').textContent = `${cities} cit${cities === 1 ? 'y' : 'ies'}`;
  document.getElementById('raised').textContent = `${rs(spots.reduce((t, s) => t + s.fundedAmount, 0))} raised`;
}

// The spot list (city -> locality -> spot) and detail panel, used on every page.
//   filter(s)   which spots appear in the list (the detail panel can still show any spot)
//   actions(s)  HTML for this page's form in the detail panel, or ''
//   onShow(s, detail)  runs after a spot is shown, e.g. to load more about it
//   submit(form, spot, action)  sends that form and returns { id, note } to show afterwards
//   empty       text when no spot passes the filter
//   listId / detailId  the elements to draw into (a page can have more than one browser)
function spotBrowser({ filter = () => true, actions = () => '', onShow = null, submit = null,
                       empty = 'No spots reported yet.', listId = 'list', detailId = 'detail' } = {}) {
  const list = document.getElementById(listId);
  const detail = document.getElementById(detailId);
  let spots = [];
  let shownId = null, shownPercent = null;

  function setNote(text, isError) {
    const el = detail.querySelector('.note');
    if (!el) return;
    el.textContent = text;
    el.classList.toggle('err', !!isError);
  }

  function show(id) {
    const s = spots.find(x => x.id === id);
    if (!s) return;
    list.querySelectorAll('li').forEach(li => li.classList.toggle('on', +li.dataset.id === id));

    // Same spot shown again (after a donation): start the bar where it was
    const from = shownId === id ? shownPercent : null;

    detail.innerHTML = `
      <img src="/api/photo/${s.id}" alt="">
      ${fundBlock(s, from)}
      ${actions(s)}
      <dl>
        <dt>Spot</dt><dd>#${s.id}</dd>
        <dt>Where</dt><dd>${esc(s.city) || '—'}, ${esc(s.locality) || '—'}</dd>
        <dt>GPS</dt><dd><a href="${mapsLink(s)}" target="_blank">${s.latitude.toFixed(4)}, ${s.longitude.toFixed(4)} ↗</a></dd>
        <dt>Waste</dt><dd>${esc(s.wasteType)}</dd>
        <dt>Size</dt><dd>${s.size}, ${s.workersNeeded} workers</dd>
        <dt>Status</dt><dd>${s.status}</dd>
        <dt>Reporter</dt><dd>${s.reporterID}</dd>
        ${s.moderatorID ? `<dt>Moderator</dt><dd>${s.moderatorID}</dd>` : ''}
        ${s.status === 'Funded' ? `<dt>Applicants</dt><dd>${s.applicants}</dd>` : ''}
        ${s.assignedWorkers?.length ? `<dt>Assigned</dt><dd>${s.assignedWorkers.join(', ')}</dd>` : ''}
      </dl>`;

    // Grow the bar to its new width on the next frame, so the change animates
    const fill = detail.querySelector('.fill');
    if (fill) requestAnimationFrame(() => requestAnimationFrame(() => { fill.style.width = fill.dataset.to + '%'; }));

    shownId = id;
    shownPercent = isFunded(s) ? 100 : percent(s);

    if (onShow) onShow(s, detail);
  }

  function renderList(visible) {
    if (!visible.length) {
      list.innerHTML = `<li class="empty">${esc(empty)}</li>`;
      return;
    }

    // Group spots: city -> locality -> spots
    const groups = new Map();
    for (const s of visible) {
      const city = s.city || 'Unknown';
      const locality = s.locality || 'Unknown';
      if (!groups.has(city)) groups.set(city, new Map());
      const localities = groups.get(city);
      if (!localities.has(locality)) localities.set(locality, []);
      localities.get(locality).push(s);
    }

    // A–Z, with "Unknown" last
    const byName = (a, b) => (a === 'Unknown') - (b === 'Unknown') || a.localeCompare(b);

    let html = '';
    for (const city of [...groups.keys()].sort(byName)) {
      const localities = groups.get(city);
      const spotCount = [...localities.values()].reduce((n, l) => n + l.length, 0);
      html += `<li class="city">${esc(city)} <em>${localities.size} localit${localities.size === 1 ? 'y' : 'ies'} · ${spotCount} spot${spotCount === 1 ? '' : 's'}</em></li>`;

      for (const locality of [...localities.keys()].sort(byName)) {
        const items = localities.get(locality);
        html += `<li class="loc">${esc(locality)} <em>${items.length}</em></li>`;
        html += items.map(s =>
          `<li class="spot" data-id="${s.id}"><span class="name">#${s.id} ${esc(s.wasteType)} <em>(${s.size.toLowerCase()})</em></span>${miniBar(s)}</li>`
        ).join('');
      }
    }

    list.innerHTML = html;
    list.querySelectorAll('li[data-id]').forEach(li => li.addEventListener('click', () => show(+li.dataset.id)));
  }

  // Fetch the spots and draw the page. selectId: the spot to open (e.g. one just reported).
  async function load(selectId) {
    try {
      spots = await (await fetch('/api/spots')).json();
    } catch {
      list.innerHTML = '<li class="empty">Could not reach the server. Is it running?</li>';
      return;
    }

    showStats(spots);
    const visible = spots.filter(filter);
    renderList(visible);

    const pick = selectId || (visible.length ? visible[visible.length - 1].id : null);
    if (pick) show(pick);
    else detail.innerHTML = '';
  }

  detail.addEventListener('click', (e) => {
    // Quick amount buttons fill in the amount
    const chip = e.target.closest('.chip[data-amount]');
    if (chip) detail.querySelector('input[name=amount]').value = chip.dataset.amount;

    // Remember Approve / Reject, for browsers without e.submitter
    const choice = e.target.closest('button[value]');
    if (choice) choice.form.dataset.action = choice.value;
  });

  if (submit) {
    detail.addEventListener('submit', async (e) => {
      e.preventDefault();
      const form = e.target;
      const spot = spots.find(x => x.id === shownId);
      const buttons = form.querySelectorAll('button');
      buttons.forEach(b => b.disabled = true);
      setNote('Saving…');

      try {
        const action = e.submitter?.value || form.dataset.action || '';
        const { id, note } = await submit(form, spot, action);
        await load(id);
        // A finished spot has no form left, so the note goes under its funding bar
        if (detail.querySelector('.note')) setNote(note);
        else detail.querySelector('.fund p').textContent = note;
      } catch (err) {
        setNote(err.message, true);
        buttons.forEach(b => b.disabled = false);
      }
    });
  }

  return { load };
}
