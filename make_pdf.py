#!/usr/bin/env python3
"""Convert the combined markdown analysis to a self-contained HTML (print to PDF)."""
import re, pathlib

MD_PATH = pathlib.Path(__file__).parent / "PINTOS_COMPLETE_ANALYSIS.md"
OUT_PATH = pathlib.Path(__file__).parent / "PintOS_Analysis.html"

md = MD_PATH.read_text(encoding="utf-8")

# Convert mermaid blocks
def mermaid_replace(m):
    code = m.group(1).strip()
    return f'<div class="mermaid">{code}</div>'

html_body = md

# Escape HTML in non-code parts minimally
# Convert ```mermaid blocks
html_body = re.sub(r'```mermaid\s*\n(.*?)```', mermaid_replace, html_body, flags=re.DOTALL)

# Convert regular code blocks
def code_replace(m):
    lang = m.group(1) or ""
    code = m.group(2).replace("&","&amp;").replace("<","&lt;").replace(">","&gt;")
    return f'<pre><code class="{lang}">{code}</code></pre>'

html_body = re.sub(r'```(\w*)\s*\n(.*?)```', code_replace, html_body, flags=re.DOTALL)

# Convert inline code
html_body = re.sub(r'`([^`]+)`', r'<code>\1</code>', html_body)

# Convert headers
for i in range(6, 0, -1):
    pat = r'^' + '#'*i + r' (.+)$'
    html_body = re.sub(pat, rf'<h{i}>\1</h{i}>', html_body, flags=re.MULTILINE)

# Convert bold
html_body = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', html_body)

# Convert tables
def table_replace(m):
    lines = m.group(0).strip().split('\n')
    rows = [l for l in lines if not re.match(r'^\|[\s\-:|]+\|$', l)]
    html = '<table>'
    for i, row in enumerate(rows):
        cells = [c.strip() for c in row.strip('|').split('|')]
        tag = 'th' if i == 0 else 'td'
        html += '<tr>' + ''.join(f'<{tag}>{c}</{tag}>' for c in cells) + '</tr>'
    html += '</table>'
    return html

html_body = re.sub(r'(\|.+\|(?:\n\|.+\|)+)', table_replace, html_body)

# Convert blockquotes
html_body = re.sub(r'^>\s*(.+)$', r'<blockquote>\1</blockquote>', html_body, flags=re.MULTILINE)

# Convert --- to hr
html_body = re.sub(r'^---+$', '<hr>', html_body, flags=re.MULTILINE)

# Convert newlines to br in non-html areas (simple)
html_body = re.sub(r'\n\n', '</p><p>', html_body)

html = f"""<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<title>PintOS 프로젝트 완전 분석</title>
<script src="https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js"></script>
<script>mermaid.initialize({{startOnLoad:true, theme:'default', securityLevel:'loose'}});</script>
<style>
@import url('https://fonts.googleapis.com/css2?family=Noto+Sans+KR:wght@300;400;500;700&family=JetBrains+Mono:wght@400;500&display=swap');
* {{ margin:0; padding:0; box-sizing:border-box; }}
body {{ font-family:'Noto Sans KR',sans-serif; font-size:11pt; line-height:1.7;
  color:#1a1a2e; max-width:900px; margin:0 auto; padding:30px; }}
h1 {{ font-size:24pt; color:#0f3460; border-bottom:3px solid #0f3460; padding-bottom:8px; margin:30px 0 15px; }}
h2 {{ font-size:18pt; color:#16213e; border-bottom:2px solid #e94560; padding-bottom:5px; margin:25px 0 12px; }}
h3 {{ font-size:14pt; color:#533483; margin:20px 0 10px; }}
h4 {{ font-size:12pt; color:#e94560; margin:15px 0 8px; }}
p {{ margin:8px 0; }}
table {{ border-collapse:collapse; width:100%; margin:12px 0; font-size:10pt; }}
th {{ background:#0f3460; color:white; padding:8px 10px; text-align:left; }}
td {{ border:1px solid #ddd; padding:6px 10px; }}
tr:nth-child(even) {{ background:#f5f5f5; }}
pre {{ background:#1a1a2e; color:#e0e0e0; padding:14px; border-radius:6px;
  overflow-x:auto; margin:10px 0; font-size:9.5pt; }}
code {{ font-family:'JetBrains Mono',monospace; font-size:9.5pt; background:#eef; padding:1px 4px; border-radius:3px; }}
pre code {{ background:none; padding:0; color:#e0e0e0; }}
blockquote {{ background:#fff3cd; border-left:4px solid #e94560; padding:8px 14px; margin:10px 0; font-size:10.5pt; }}
hr {{ border:none; border-top:2px solid #ccc; margin:20px 0; }}
.mermaid {{ margin:15px auto; text-align:center; }}
strong {{ color:#0f3460; }}
@media print {{
  body {{ font-size:10pt; padding:15px; }}
  pre {{ font-size:8.5pt; }}
  .mermaid {{ page-break-inside:avoid; }}
  h1,h2,h3 {{ page-break-after:avoid; }}
}}
</style>
</head>
<body>
<p>{html_body}</p>
</body>
</html>"""

OUT_PATH.write_text(html, encoding="utf-8")
print(f"[OK] HTML done: {OUT_PATH}")
print("Open in browser -> Ctrl+P -> Save as PDF")
print(f"  Size: {OUT_PATH.stat().st_size:,} bytes")
