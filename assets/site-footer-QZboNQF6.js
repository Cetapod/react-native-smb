(function(){const o=document.createElement("link").relList;if(o&&o.supports&&o.supports("modulepreload"))return;for(const r of document.querySelectorAll('link[rel="modulepreload"]'))a(r);new MutationObserver(r=>{for(const i of r)if(i.type==="childList")for(const l of i.addedNodes)l.tagName==="LINK"&&l.rel==="modulepreload"&&a(l)}).observe(document,{childList:!0,subtree:!0});function s(r){const i={};return r.integrity&&(i.integrity=r.integrity),r.referrerPolicy&&(i.referrerPolicy=r.referrerPolicy),r.crossOrigin==="use-credentials"?i.credentials="include":r.crossOrigin==="anonymous"?i.credentials="omit":i.credentials="same-origin",i}function a(r){if(r.ep)return;r.ep=!0;const i=s(r);fetch(r.href,i)}})();const d={name:"@cetapod/react-native-smb",version:"1.0.0",github:"https://github.com/cetapod/react-native-smb",npm:"https://www.npmjs.com/package/@cetapod/react-native-smb",example:"https://github.com/cetapod/react-native-smb/tree/main/example",license:"https://github.com/cetapod/react-native-smb/blob/main/LICENSE",org:"Cetapod",tagline:"Native SMB client for React Native via libsmb2 and Nitro Modules."},t={home:"/react-native-smb/",docs:"/react-native-smb/docs/",docsStart:"/react-native-smb/docs/",docsTasks:"/react-native-smb/docs/tasks.html",docsApi:"/react-native-smb/docs/api.html",docsTypes:"/react-native-smb/docs/types.html",docsArchitecture:"/react-native-smb/docs/architecture.html"},k=[{href:t.docsStart,label:"Getting started",id:"start"},{href:t.docsTasks,label:"Task model",id:"tasks"},{href:t.docsApi,label:"API reference",id:"api"},{href:t.docsTypes,label:"Types & enums",id:"types"},{href:t.docsArchitecture,label:"Architecture",id:"architecture"}],_=[{title:"Getting started",href:t.docsStart,section:"Guide",keywords:"install quick start npm pod connect url compatibility"},{title:"Task model",href:t.docsTasks,section:"Guide",keywords:"smbtask subscribe cancel result progress hooks transfer tray"},{title:"API reference",href:t.docsApi,section:"Reference",keywords:"methods smb client filesystem pool hooks"},{title:"Types & enums",href:t.docsTypes,section:"Reference",keywords:"interfaces enums classes smbcredentials smbfileinfo"},{title:"Architecture",href:t.docsArchitecture,section:"Guide",keywords:"connection pool slots pipeline multichannel libsmb2"},{title:"Install",href:`${t.docsStart}#install`,section:"Getting started",keywords:"npm install nitro pod"},{title:"Quick start",href:`${t.docsStart}#quick-start`,section:"Getting started",keywords:"connect list download example"},{title:"URL format",href:`${t.docsStart}#url-format`,section:"Getting started",keywords:"smb url domain username share"},{title:"Watch progress",href:`${t.docsTasks}#subscribe`,section:"Task model",keywords:"subscribe progress bytes status"},{title:"useSubscribe hook",href:`${t.docsTasks}#subscribe`,section:"Task model",keywords:"react hook subscribe cancel"},{title:"Transfer tray",href:`${t.docsTasks}#transfer-tray`,section:"Task model",keywords:"useTransfers useTransferActions tray"},{title:"Error handling",href:`${t.docsTasks}#errors`,section:"Task model",keywords:"smberror smbtaskerror"},{title:"SMB()",href:`${t.docsApi}#smb`,section:"API",keywords:"constructor client instance"},{title:"initialize",href:`${t.docsApi}#initialize`,section:"API",keywords:"init libsmb2"},{title:"connect",href:`${t.docsApi}#connect`,section:"API",keywords:"connection url credentials share"},{title:"connectShare",href:`${t.docsApi}#connectshare`,section:"API",keywords:"mount share"},{title:"disconnect",href:`${t.docsApi}#disconnect`,section:"API",keywords:"close connection"},{title:"listShares",href:`${t.docsApi}#listshares`,section:"API",keywords:"shares enumerate"},{title:"listDirectory",href:`${t.docsApi}#listdirectory`,section:"API",keywords:"list dir folder browse"},{title:"getPathInfo",href:`${t.docsApi}#getpathinfo`,section:"API",keywords:"stat metadata file info"},{title:"downloadFile",href:`${t.docsApi}#downloadfile`,section:"API",keywords:"download transfer progress"},{title:"uploadFile",href:`${t.docsApi}#uploadfile`,section:"API",keywords:"upload transfer progress"},{title:"createDirectory",href:`${t.docsApi}#createdirectory`,section:"API",keywords:"mkdir folder"},{title:"deleteItem",href:`${t.docsApi}#deleteitem`,section:"API",keywords:"delete remove file folder"},{title:"renameItem",href:`${t.docsApi}#renameitem`,section:"API",keywords:"rename"},{title:"moveItem",href:`${t.docsApi}#moveitem`,section:"API",keywords:"move"},{title:"copyItem",href:`${t.docsApi}#copyitem`,section:"API",keywords:"copy duplicate tree"},{title:"duplicateItem",href:`${t.docsApi}#duplicateitem`,section:"API",keywords:"duplicate clone"},{title:"getSecurityDescriptor",href:`${t.docsApi}#getsecuritydescriptor`,section:"API",keywords:"acl security windows ntfs"},{title:"getPoolInfo",href:`${t.docsApi}#getpoolinfo`,section:"API",keywords:"pool slots debug"},{title:"subscribePoolInfo",href:`${t.docsApi}#subscribepoolinfo`,section:"API",keywords:"pool subscribe live"},{title:"resetPool / dispose",href:`${t.docsApi}#resetpool-dispose`,section:"API",keywords:"dispose cleanup lifecycle"},{title:"useSubscribe",href:`${t.docsApi}#usesubscribe`,section:"API",keywords:"hook react subscribe"},{title:"useTransfers",href:`${t.docsApi}#usetransfers-useTransferactions`,section:"API",keywords:"hook transfer tray actions"},{title:"SmbCredentials",href:`${t.docsTypes}#smbcredentials`,section:"Types",keywords:"username password domain"},{title:"SmbConnectionInfo",href:`${t.docsTypes}#smbconnectioninfo`,section:"Types",keywords:"connection share host"},{title:"SmbFileInfo",href:`${t.docsTypes}#smbfileinfo`,section:"Types",keywords:"file directory size mtime"},{title:"SmbTaskState",href:`${t.docsTypes}#smb-task-state`,section:"Types",keywords:"progress status bytes"},{title:"PoolInfo",href:`${t.docsTypes}#poolinfo-poolslotinfo`,section:"Types",keywords:"pool slot interactive"},{title:"TaskStatus",href:`${t.docsTypes}#taskstatus`,section:"Types",keywords:"enum pending running completed"},{title:"SmbError",href:`${t.docsTypes}#smberror`,section:"Types",keywords:"error code native"},{title:"SmbOperatorKind",href:`${t.docsTypes}#smboperatorkind`,section:"Types",keywords:"operator kind transfer"},{title:"SmbTask",href:`${t.docsTypes}#smbtask`,section:"Types",keywords:"class result subscribe cancel"},{title:"SmbTaskError",href:`${t.docsTypes}#smbtaskerror`,section:"Types",keywords:"task error"},{title:"Connection pool",href:`${t.docsArchitecture}#pool`,section:"Architecture",keywords:"slots interactive priority"},{title:"Chunk pipeline",href:`${t.docsArchitecture}#pipeline`,section:"Architecture",keywords:"pipelining read write inflight"}];let n=null,p=null,m=null,u=0,f=[];function T(){return n||(n=document.createElement("div"),n.className="doc-search",n.dataset.docSearch="",n.setAttribute("aria-hidden","true"),n.innerHTML=`
    <div class="doc-search__backdrop" data-doc-search-close></div>
    <div class="doc-search__panel" role="dialog" aria-modal="true" aria-label="Search documentation">
      <div class="doc-search__header">
        <svg class="doc-search__icon" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><circle cx="11" cy="11" r="8"/><path d="M21 21l-4.35-4.35"/></svg>
        <input type="search" class="doc-search__input" placeholder="Search docs — methods, types, guides…" data-doc-search-input autocomplete="off" />
        <kbd class="doc-search__kbd">Esc</kbd>
      </div>
      <div class="doc-search__results" data-doc-search-results role="listbox"></div>
      <div class="doc-search__footer">
        <span><kbd>↑</kbd><kbd>↓</kbd> navigate</span>
        <span><kbd>↵</kbd> open</span>
        <span><kbd>/</kbd> or <kbd>⌘K</kbd> search</span>
      </div>
    </div>
  `,document.body.appendChild(n),p=n.querySelector("[data-doc-search-input]"),m=n.querySelector("[data-doc-search-results]"),n.querySelectorAll("[data-doc-search-close]").forEach(e=>{e.addEventListener("click",v)}),p.addEventListener("input",()=>{u=0,$(p.value)}),p.addEventListener("keydown",e=>{e.key==="ArrowDown"?(e.preventDefault(),u=Math.min(u+1,f.length-1),g()):e.key==="ArrowUp"?(e.preventDefault(),u=Math.max(u-1,0),g()):e.key==="Enter"&&f[u]?(e.preventDefault(),A(f[u].href)):e.key==="Escape"&&v()}),n)}function b(e,o){return`${e.title} ${e.keywords} ${e.section}`.toLowerCase().includes(o)?e.title.toLowerCase().startsWith(o)?3:e.title.toLowerCase().includes(o)?2:1:0}function $(e){const o=e.trim().toLowerCase();if(f=o?_.filter(s=>b(s,o)>0).sort((s,a)=>b(a,o)-b(s,o)).slice(0,12):_.filter(s=>s.section==="Guide"||s.section==="Reference").slice(0,8),!f.length){m.innerHTML=`<p class="doc-search__empty">No results for “${e.trim()}”</p>`;return}m.innerHTML=f.map((s,a)=>`
        <a href="${s.href}" class="doc-search__result${a===u?" doc-search__result--active":""}" role="option" data-index="${a}">
          <span class="doc-search__result-title">${s.title}</span>
          <span class="doc-search__result-section">${s.section}</span>
        </a>
      `).join(""),m.querySelectorAll(".doc-search__result").forEach(s=>{s.addEventListener("click",a=>{a.preventDefault(),A(s.getAttribute("href"))})})}function g(){m.querySelectorAll(".doc-search__result").forEach((e,o)=>{e.classList.toggle("doc-search__result--active",o===u),o===u&&e.scrollIntoView({block:"nearest"})})}function A(e){v(),window.location.href=e}function y(){T(),n.removeAttribute("aria-hidden"),n.classList.add("doc-search--open"),document.body.classList.add("doc-search-open"),u=0,p.value="",$(""),requestAnimationFrame(()=>p.focus())}function v(){n&&(n.classList.remove("doc-search--open"),n.setAttribute("aria-hidden","true"),document.body.classList.remove("doc-search-open"))}function S(){document.addEventListener("click",e=>{e.target.closest("[data-doc-search-open]")&&(e.preventDefault(),y())}),document.addEventListener("keydown",e=>{var a,r,i,l,h,c;const o=(a=document.activeElement)==null?void 0:a.tagName,s=o==="INPUT"||o==="TEXTAREA"||((r=document.activeElement)==null?void 0:r.isContentEditable);if((e.metaKey||e.ctrlKey)&&e.key==="k"){e.preventDefault(),n!=null&&n.classList.contains("doc-search--open")?v():y();return}if(e.key==="/"&&!e.metaKey&&!e.ctrlKey&&!e.altKey){if(s&&!((l=(i=document.activeElement)==null?void 0:i.dataset)!=null&&l.docSearchInput)||(c=(h=document.activeElement)==null?void 0:h.dataset)!=null&&c.pageFilter)return;e.preventDefault(),y();return}e.key==="Escape"&&(n!=null&&n.classList.contains("doc-search--open"))&&v()})}const w="docs-theme";function L(){return`
    <button type="button" class="nav__theme" data-theme-toggle aria-label="Toggle dark mode">
      <svg class="nav__theme-sun" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><circle cx="12" cy="12" r="5"/><path d="M12 1v2M12 21v2M4.22 4.22l1.42 1.42M18.36 18.36l1.42 1.42M1 12h2M21 12h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42"/></svg>
      <svg class="nav__theme-moon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M21 12.79A9 9 0 1111.21 3 7 7 0 0021 12.79z"/></svg>
    </button>
  `}function D(e){const{mode:o,docsPage:s}=e,a=document.querySelector("[data-site-nav]");if(!a)return;const r=o==="docs",i=t.home,l=t.docs,h=[{href:`${i}#features`,label:"Features",key:"features"},{href:`${i}#use-cases`,label:"Use cases",key:"use-cases",hideMd:!0},{href:`${i}#architecture`,label:"Pool",key:"architecture"},{href:`${i}#code`,label:"Examples",key:"code",hideMd:!0}];a.classList.add(r?"nav--docs":"nav--home"),a.innerHTML=`
    <div class="nav__inner">
      <a href="${i}" class="nav__brand">
        <img src="/react-native-smb/favicon.svg" alt="" width="36" height="36" />
        <span class="nav__brand-text">
          <span class="nav__brand-org">${d.org}</span>
          <span class="nav__brand-name">react-native-smb</span>
        </span>
      </a>

      <div class="nav__pill">
        ${h.map(c=>`
              <a href="${c.href}" class="nav__link${c.hideMd?" nav__link--hide-md":""}" data-nav-key="${c.key}">${c.label}</a>
            `).join("")}
        <span class="nav__divider" aria-hidden="true"></span>
        <div class="nav__docs-wrap" data-docs-dropdown>
          <a href="${l}" class="nav__link nav__link--docs${r?" nav__link--active":""}" data-nav-key="docs" aria-haspopup="true" aria-expanded="false">
            <span>Docs</span>
            <svg class="nav__docs-chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M6 9l6 6 6-6"/></svg>
          </a>
          <div class="nav__docs-menu" role="menu">
            ${k.map(c=>`
                <a href="${c.href}" role="menuitem"
                  class="nav__docs-item${c.id===s?" nav__docs-item--active":""}">
                  ${c.label}
                </a>
              `).join("")}
          </div>
        </div>
      </div>

      <div class="nav__actions">
        <button type="button" class="nav__icon-btn" data-doc-search-open aria-label="Search documentation" title="Search docs (⌘K)">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><circle cx="11" cy="11" r="8"/><path d="M21 21l-4.35-4.35"/></svg>
        </button>
        ${L()}
        <a href="${d.npm}" class="nav__icon-btn" target="_blank" rel="noopener" aria-label="npm">
          <svg viewBox="0 0 24 24" fill="currentColor"><path d="M1.763 0C.786 0 0 .786 0 1.763v20.474C0 23.214.786 24 1.763 24h20.474c.977 0 1.763-.786 1.763-1.763V1.763C24 .786 23.214 0 22.237 0zM5.13 5.323l13.837.019-1.834 13.47L5.13 5.323z"/></svg>
        </a>
        <a href="${d.github}" class="btn btn--ghost nav__github" target="_blank" rel="noopener">
          <svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 0C5.37 0 0 5.37 0 12c0 5.31 3.435 9.795 8.205 11.385.6.105.825-.255.825-.57 0-.285-.015-1.23-.015-2.235-3.015.555-3.795-.735-4.035-1.41-.135-.345-.72-1.41-1.23-1.695-.42-.225-1.02-.78-.015-.795.945-.015 1.62.87 1.845 1.23 1.08 1.815 2.805 1.305 3.495.99.105-.78.42-1.305.765-1.605-2.67-.3-5.46-1.335-5.46-5.925 0-1.305.465-2.385 1.23-3.225-.12-.3-.54-1.53.12-3.18 0 0 1.005-.315 3.3 1.23.96-.27 1.98-.405 3 .405s2.04.135 3 .405c2.295-1.56 3.3-1.23 3.3-1.23.66 1.65.24 2.88.12 3.18.765.84 1.23 1.905 1.23 3.225 0 4.605-2.805 5.625-5.475 5.925.435.375.81 1.095.81 2.22 0 1.605-.015 2.895-.015 3.3 0 .315.225.69.825.57A12.02 12.02 0 0024 12c0-6.63-5.37-12-12-12z"/></svg>
          GitHub
        </a>
        ${r?`<button type="button" class="nav__sidebar-toggle" data-docs-menu aria-label="Open docs menu">
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M4 6h16M4 12h16M4 18h16"/></svg>
              </button>`:""}
        <button type="button" class="nav__toggle" data-nav-menu aria-label="Open menu" aria-expanded="false">
          <span></span><span></span><span></span>
        </button>
      </div>
    </div>

    <div class="nav__mobile" data-nav-mobile>
      ${h.map(c=>`<a href="${c.href}">${c.label}</a>`).join("")}
      <div class="nav__mobile-docs">
        <span class="nav__mobile-label">Documentation</span>
        ${k.map(c=>`<a href="${c.href}"${c.id===s?' class="nav__mobile--active"':""}>${c.label}</a>`).join("")}
      </div>
      <a href="${d.github}" target="_blank" rel="noopener">GitHub</a>
      <a href="${d.npm}" target="_blank" rel="noopener">npm</a>
    </div>
  `,M(a),x(a),I(a),E(a),S(),r?(a.classList.add("nav--scrolled"),P()):C(a)}function E(e){let o=window.scrollY,s=!1;const a=()=>{const r=window.scrollY,i=r-o,l=e.querySelector(".nav__mobile--open"),h=document.body.classList.contains("doc-search-open");if(l||h){e.classList.remove("nav--hidden"),document.body.classList.remove("nav-is-hidden"),o=r,s=!1;return}r<64?(e.classList.remove("nav--hidden"),document.body.classList.remove("nav-is-hidden")):i>6?(e.classList.add("nav--hidden"),document.body.classList.add("nav-is-hidden")):i<-6&&(e.classList.remove("nav--hidden"),document.body.classList.remove("nav-is-hidden")),o=r,s=!1};window.addEventListener("scroll",()=>{s||(requestAnimationFrame(a),s=!0)},{passive:!0})}function I(e){const o=e.querySelector("[data-docs-dropdown]");if(!o)return;const s=o.querySelector(".nav__link--docs"),a=r=>{o.classList.toggle("nav__docs-wrap--open",r),s==null||s.setAttribute("aria-expanded",String(r))};o.addEventListener("mouseenter",()=>{window.innerWidth>1100&&a(!0)}),o.addEventListener("mouseleave",()=>a(!1)),s==null||s.addEventListener("keydown",r=>{var i;r.key==="Escape"&&a(!1),r.key==="ArrowDown"&&window.innerWidth>1100&&(r.preventDefault(),a(!0),(i=o.querySelector(".nav__docs-item"))==null||i.focus())}),document.addEventListener("click",r=>{o.contains(r.target)||a(!1)})}function P(){if(document.querySelector(".reading-progress"))return;const e=document.createElement("div");e.className="reading-progress",e.innerHTML='<div class="reading-progress__fill" data-reading-fill></div>',document.body.appendChild(e);const o=e.querySelector("[data-reading-fill]"),s=()=>{const a=document.documentElement,r=a.scrollHeight-a.clientHeight,i=r>0?a.scrollTop/r*100:0;o.style.width=`${i}%`};window.addEventListener("scroll",s,{passive:!0}),s()}function M(e){var r;const o=document.documentElement,s=localStorage.getItem(w),a=window.matchMedia("(prefers-color-scheme: dark)").matches;o.getAttribute("data-theme")||o.setAttribute("data-theme",s||(a?"dark":"light")),(r=e.querySelector("[data-theme-toggle]"))==null||r.addEventListener("click",()=>{const i=o.getAttribute("data-theme")==="dark"?"light":"dark";o.setAttribute("data-theme",i),localStorage.setItem(w,i)})}function x(e){const o=e.querySelector("[data-nav-menu]"),s=e.querySelector("[data-nav-mobile]");o==null||o.addEventListener("click",()=>{const a=s==null?void 0:s.classList.toggle("nav__mobile--open");o.setAttribute("aria-expanded",String(a))}),s==null||s.querySelectorAll("a").forEach(a=>{a.addEventListener("click",()=>{s.classList.remove("nav__mobile--open"),o==null||o.setAttribute("aria-expanded","false")})})}function C(e){const o=document.querySelectorAll("section[id], .trust-bar[id]"),s=()=>{const a=window.scrollY;e.classList.toggle("nav--scrolled",a>40);let r="";o.forEach(i=>{a>=i.offsetTop-120&&(r=i.id)}),e.querySelectorAll(".nav__pill .nav__link[data-nav-key]").forEach(i=>{const l=i.dataset.navKey,h=l==="docs"?!1:l===r;i.classList.toggle("nav__link--active",h)})};window.addEventListener("scroll",s,{passive:!0}),s()}function q(e={}){const{mode:o="home"}=e,s=document.querySelector("[data-site-footer]");if(!s)return;const a=o==="docs";s.innerHTML=`
    <footer class="footer">
      <div class="container">
        <div class="footer__inner">
          <div class="footer__brand">
            <div class="footer__brand-logo">
              <img src="/react-native-smb/favicon.svg" alt="" width="32" height="32" />
              ${d.name}
            </div>
            <p>
              ${d.tagline}
              Built by
              <a href="mailto:open-source@cetapod.com">${d.org} open source</a>.
            </p>
          </div>
          <div class="footer__links">
            <div class="footer__col">
              <h4>Project</h4>
              <a href="${d.github}" target="_blank" rel="noopener">GitHub</a>
              <a href="${d.npm}" target="_blank" rel="noopener">npm</a>
              <a href="${d.example}" target="_blank" rel="noopener">Example app</a>
              <a href="${d.license}" target="_blank" rel="noopener">License (LGPL-2.1)</a>
            </div>
            <div class="footer__col">
              <h4>Docs</h4>
              <a href="${t.docsStart}">Getting started</a>
              <a href="${t.docsApi}">API reference</a>
              <a href="${t.docsTasks}">Task model</a>
              <a href="${t.docsTypes}">Types & enums</a>
              <a href="${t.docsArchitecture}">Architecture</a>
            </div>
            <div class="footer__col">
              <h4>Related</h4>
              <a href="https://github.com/sahlberg/libsmb2" target="_blank" rel="noopener">libsmb2</a>
              <a href="https://nitro.margelo.com" target="_blank" rel="noopener">Nitro Modules</a>
            </div>
          </div>
        </div>
        <div class="footer__bottom">
          <span>&copy; 2026 ${d.org}. Open source under LGPL-2.1.</span>
          ${a?`<a href="${t.home}">Back to home</a>`:`<span>v${d.version}</span>`}
        </div>
      </div>
    </footer>
  `}export{k as D,t as P,d as S,q as a,D as i};
