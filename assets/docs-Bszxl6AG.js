import{i as q,a as E,P as $,c as v,L as h,D as y,b as f,d as C}from"./site-footer-TBKdJ_kK.js";function g(e){return e.toLowerCase().replace(/[()]/g,"").replace(/[^a-z0-9]+/g,"-").replace(/^-|-$/g,"")}function _(e){var a,r;if(e.id)return e.id;const o=((a=e.querySelector(".method-card__name"))==null?void 0:a.textContent)||((r=e.querySelector(".type-card__name"))==null?void 0:r.textContent)||"section";let t=g(o),s=2;for(;document.getElementById(t);)t=`${g(o)}-${s++}`;return e.id=t,t}function k(){var a,r;const e=document.querySelector(".docs-content");if(!e)return[];const o=[];let t=null;const s=(n,c)=>{t&&(t.items.some(i=>i.id===n)||t.items.push({id:n,title:c}))};for(const n of e.children){if(n.matches("h2[id]")){t={id:n.id,title:n.textContent.trim(),items:[]},o.push(t);continue}if(t){if(n.matches("h3[id]"))s(n.id,n.textContent.trim());else if(n.matches(".method-card, .type-card")){const c=_(n),i=((a=n.querySelector(".method-card__name"))==null?void 0:a.textContent.trim())||((r=n.querySelector(".type-card__name"))==null?void 0:r.textContent.trim())||c;s(c,i)}}}return o}function w(){const e=document.querySelector(".docs-sidebar");if(!e||e.querySelector(".docs-sidebar__home"))return;const o=document.createElement("a");o.href=$.home,o.className="docs-sidebar__home",o.innerHTML=`
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="16" height="16"><path d="M19 12H5M12 19l-7-7 7-7"/></svg>
    Site overview
  `,e.insertBefore(o,e.firstChild)}function x(){var n;const e=document.querySelector(".docs-content"),o=e==null?void 0:e.querySelector("h1"),t=document.body.dataset.docsPage,s=y.find(c=>c.id===t);if(!o||!s)return;const a=v(),r=document.createElement("div");r.className="docs-page-meta",r.innerHTML=`
    <span class="docs-page-meta__crumb">Docs <span aria-hidden="true">›</span> ${s.label}</span>
    <span class="docs-page-meta__actions">
      <label class="docs-version">
        <span class="docs-version__sr">Documentation version</span>
        <select class="docs-version__select" data-docs-version-select>
          ${f.map(c=>`
              <option value="${c.id}"${c.id===a.id?" selected":""}>
                ${c.label}${c===h?" (latest)":""}
              </option>
            `).join("")}
        </select>
      </label>
    </span>
  `,o.before(r),(n=r.querySelector("[data-docs-version-select]"))==null||n.addEventListener("change",c=>{const i=f.find(l=>l.id===c.target.value);i&&(window.location.href=S(i,t))})}function S(e,o){const t=C(e);return`${(t.find(a=>a.id===o)??t[0]).href}${window.location.hash}`}function T(){const e=v(),o=document.querySelector(".docs-content");if(e===h||!o)return;const t=document.createElement("div");t.className="docs-version-banner",t.innerHTML=`
    You're viewing the docs for <strong>${e.label}</strong>.
    <a href="${S(h,document.body.dataset.docsPage)}">Go to the latest (${h.label}) →</a>
  `,o.prepend(t)}function A(){const e=document.body.dataset.docsPage,o=document.querySelector("[data-docs-nav]");o&&(o.innerHTML=`
    <div class="docs-nav__label">Documentation</div>
    ${y.map(t=>`
        <a href="${t.href}" class="docs-nav__link${t.id===e?" docs-nav__link--active":""}">
          ${t.label}
        </a>
      `).join("")}
  `)}function B(e){var t;const o=document.querySelector("[data-docs-toc-right]");if(!o||!e.length){(t=document.querySelector(".docs-body-wrap"))==null||t.classList.add("docs-body-wrap--no-rail");return}o.innerHTML=`
    <div class="docs-rail__inner">
      <div class="docs-rail__label">Contents</div>
      <nav class="docs-rail__nav">
        ${e.map(s=>`
              <div class="docs-rail__section" data-section="${s.id}">
                <a href="#${s.id}" class="docs-rail__link docs-rail__link--h2" data-toc-id="${s.id}">${s.title}</a>
                ${s.items.length?`<div class="docs-rail__children">
                        ${s.items.map(a=>`
                              <a href="#${a.id}" class="docs-rail__link docs-rail__link--child" data-toc-id="${a.id}">${a.title}</a>
                            `).join("")}
                      </div>`:""}
              </div>
            `).join("")}
      </nav>
      <div class="docs-rail__track" aria-hidden="true">
        <div class="docs-rail__indicator" data-rail-indicator></div>
      </div>
    </div>
  `}function M(e){const o=[],t=n=>{const c=document.getElementById(n);c&&!o.some(i=>i.id===n)&&o.push({id:n,el:c})};if(e.forEach(n=>{t(n.id),n.items.forEach(c=>t(c.id))}),!o.length)return;let s=o[0].id;const a=n=>{if(s===n)return;s=n,document.querySelectorAll("[data-toc-id]").forEach(d=>{d.classList.toggle("docs-rail__link--active",d.dataset.tocId===n)});const c=document.querySelector("[data-rail-indicator]"),i=document.querySelector(".docs-rail__link--active"),l=c==null?void 0:c.parentElement;if(c&&i&&l){const d=i.getBoundingClientRect(),m=d.top-l.getBoundingClientRect().top;c.style.height=`${d.height}px`,c.style.transform=`translateY(${m}px)`}},r=()=>{let c=o[0].id;for(const{id:i,el:l}of o)l.getBoundingClientRect().top<=96&&(c=i);a(c)};window.addEventListener("scroll",r,{passive:!0}),window.addEventListener("resize",r,{passive:!0}),r(),document.querySelectorAll("[data-toc-id]").forEach(n=>{n.addEventListener("click",c=>{var d;const i=(d=n.getAttribute("href"))==null?void 0:d.slice(1),l=i?document.getElementById(i):null;l&&(c.preventDefault(),l.scrollIntoView({behavior:"smooth",block:"start"}),a(i))})})}function I(){const e=document.querySelector(".docs-sidebar"),o=document.querySelector("[data-docs-menu]"),t=document.querySelector("[data-docs-overlay]"),s=()=>{e==null||e.classList.remove("docs-sidebar--open"),t==null||t.classList.remove("docs-overlay--visible"),document.body.classList.remove("docs-sidebar-lock")};o==null||o.addEventListener("click",()=>{const a=e==null?void 0:e.classList.toggle("docs-sidebar--open");t==null||t.classList.toggle("docs-overlay--visible",a),document.body.classList.toggle("docs-sidebar-lock",!!a)}),t==null||t.addEventListener("click",s),e==null||e.querySelectorAll("a").forEach(a=>a.addEventListener("click",s))}function P(){var l;const e=document.body.dataset.docsPage;if(e!=="api"&&e!=="types")return;const o=document.querySelector(".docs-content"),t=o==null?void 0:o.querySelector(".docs-lead"),s=document.querySelectorAll("[data-searchable]"),a=document.querySelector("[data-no-results]");if(!o||!s.length)return;const r=document.createElement("div");r.className="docs-page-filter",r.innerHTML=`
    <svg class="docs-page-filter__icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"/><path d="M21 21l-4.35-4.35"/></svg>
    <input type="search" class="docs-page-filter__input" placeholder="${e==="api"?"Filter methods on this page…":"Filter types on this page…"}" data-page-filter autocomplete="off" />
    <button type="button" class="docs-page-filter__clear" data-page-filter-clear hidden>Clear</button>
    <span class="docs-page-filter__hint" aria-hidden="true">⌘K all docs</span>
  `,(l=t||o.querySelector("h1"))==null||l.after(r);const n=r.querySelector("[data-page-filter]"),c=r.querySelector("[data-page-filter-clear]"),i=()=>{const d=n.value.trim().toLowerCase();let m=0;s.forEach(u=>{const L=u.dataset.searchable||"",p=!d||L.toLowerCase().includes(d);u.classList.toggle("method-card--hidden",!p&&u.classList.contains("method-card")),u.classList.toggle("type-card--hidden",!p&&u.classList.contains("type-card")),p&&m++}),a==null||a.classList.toggle("docs-no-results--visible",d.length>0&&m===0),c.hidden=!d};n.addEventListener("input",i),c.addEventListener("click",()=>{n.value="",i(),n.focus()}),n.addEventListener("keydown",d=>{d.key==="Escape"&&(n.value="",i(),n.blur())})}function H(){document.querySelectorAll(".method-card, .type-card").forEach(e=>{if(e.querySelector("[data-copy-anchor]"))return;const o=e.id||_(e),t=document.createElement("button");t.type="button",t.className="card-anchor",t.dataset.copyAnchor=o,t.setAttribute("aria-label","Copy link to section"),t.innerHTML='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M10 13a5 5 0 007.54.54l3-3a5 5 0 00-7.07-7.07l-1.72 1.71"/><path d="M14 11a5 5 0 00-7.54-.54l-3 3a5 5 0 007.07 7.07l1.71-1.71"/></svg>';const s=e.querySelector(".method-card__header, .type-card__header");s==null||s.appendChild(t),t.addEventListener("click",async()=>{const a=`${location.origin}${location.pathname}#${o}`;try{await navigator.clipboard.writeText(a),t.classList.add("card-anchor--copied"),setTimeout(()=>t.classList.remove("card-anchor--copied"),2e3)}catch{}})})}function D(){document.querySelectorAll("[data-copy-code]").forEach(e=>{e.addEventListener("click",async()=>{var s;const o=e.closest(".docs-code"),t=(s=o==null?void 0:o.querySelector("pre"))==null?void 0:s.textContent;if(t)try{await navigator.clipboard.writeText(t);const a=e.textContent;e.textContent="Copied!",setTimeout(()=>{e.textContent=a},2e3)}catch{e.textContent="Failed"}})})}q({mode:"docs",docsPage:document.body.dataset.docsPage});E({mode:"docs"});const b=k();w();T();x();A();B(b);M(b);I();P();D();H();
