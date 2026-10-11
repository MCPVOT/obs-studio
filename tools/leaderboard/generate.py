#!/usr/bin/env python3
"""Generate leaderboard.json for obs-rust/obs-studio.

Usage: generate.py [--repo OWNER/REPO] [--out leaderboard.json]
Needs the `gh` CLI, authenticated (locally, or GH_TOKEN/GITHUB_TOKEN in CI).
Standard library only.

Crediting rules
---------------
Credit goes to the ORIGINATING contributor, not whoever rebased or merged.
  1. A merged PR credits its author (`author.login`).
  2. If the PR author is a "relay" (the repo owner, a bot or an agent account)
     the PR may carry someone else's work. Then the original person is
     credited instead, found from, in order:
       a. `Co-Authored-By:` trailers in the PR's commits and body. Emails
          like `login@users.noreply.github.com` or
          `123+login@users.noreply.github.com` map to that login; other
          emails/names are looked up through the GitHub user search and, if
          that fails, the name is kept as-is (no avatar). A trailer only
          counts if it is on at least half of the PR's commits (or in the
          body), so a large stacked PR carrying a few commits is not
          reassigned.
       b. Body text such as "Supersedes #N (by @x)", "rebase of #N by @x",
          "re-lands #N": the author of PR #N in this repo is credited.
     If nothing is found the relay itself is credited.
  3. Bots and AI agents (`[bot]` logins, is_bot, Claude/Anthropic/Copilot
     co-authors) are never credited or ranked. Each credited person gets one
     merged-PR credit per PR, however many signals name them.
  4. Issues filed: every issue (not PR) a person opened, excluding bots.
  5. score = 10 * merged_prs + 2 * issues_filed.
Closed-unmerged PRs earn nothing; their content is credited via rule 2 when
re-landed.
"""
import argparse, json, re, subprocess, sys
from datetime import datetime, timezone

RELAYS = {"zackees"}  # repo owner; also anything treated as a bot
W_PR, W_ISSUE = 10, 2
BOT_RE = re.compile(r"(\[bot\]$|^(claude|copilot|dependabot|github-actions|coderabbit)|anthropic|noreply@github\.com$)", re.I)
TRAILER = re.compile(r"(?im)^\s*co-authored-by:\s*(.+?)\s*<([^>]*)>")
REFS = re.compile(r"(?i)(?:supersedes|superseding|re-?lands?|rebase of|rebased from|carr(?:y|ies) over)\s+#(\d+)")
NOREPLY = re.compile(r"^(?:\d+\+)?([^@]+)@users\.noreply\.github\.com$", re.I)

def gh(*a):
    r = subprocess.run(["gh", *a], capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f"gh {' '.join(a)} failed: {r.stderr}")
    return r.stdout

def is_bot(login, email=""):
    return bool(BOT_RE.search(login or "") or BOT_RE.search(email or ""))

_cache = {}
def resolve(name, email, repo):
    """Map a co-author to a GitHub login (or None)."""
    m = NOREPLY.match(email)
    if m:
        return m.group(1)
    key = (name, email)
    if key not in _cache:
        login = None
        for q in (f"{email} in:email", f"{name} in:fullname"):
            try:
                d = json.loads(gh("api", "-X", "GET", "search/users", "-f", f"q={q}"))
                if d.get("total_count") == 1:
                    login = d["items"][0]["login"]; break
            except SystemExit:
                pass
        _cache[key] = login
    return _cache[key]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default="obs-rust/obs-studio")
    ap.add_argument("--out", default="leaderboard.json")
    o = ap.parse_args()
    repo = o.repo
    prs = json.loads(gh("pr", "list", "-R", repo, "--state", "all", "-L", "5000", "--json",
                        "number,state,author,body,mergedAt,createdAt"))
    issues = json.loads(gh("issue", "list", "-R", repo, "--state", "all", "-L", "5000", "--json",
                           "number,author,createdAt"))
    by_num = {p["number"]: p for p in prs}
    people = {}
    def person(login):
        return people.setdefault(login, {"login": login, "avatar_url": f"https://github.com/{login}.png?size=200",
                                         "merged_prs": 0, "issues_filed": 0, "prs": [], "issues": [], "dates": []})
    notes = []
    for p in sorted(prs, key=lambda p: p["number"]):
        if p["state"] != "MERGED":
            continue
        a = p["author"] or {}
        login = a.get("login", "")
        credited = []
        if login in RELAYS or a.get("is_bot") or is_bot(login):
            commits = json.loads(gh("pr", "view", str(p["number"]), "-R", repo, "--json", "commits"))["commits"]
            text = (p["body"] or "") + "\n" + "\n".join(
                (c.get("messageHeadline", "") + "\n" + (c.get("messageBody") or "")) for c in commits)
            seen = {}
            for c in commits:
                msg = (c.get("messageHeadline", "") + "\n" + (c.get("messageBody") or ""))
                for name, email in {(n_, e_) for n_, e_ in TRAILER.findall(msg)}:
                    if not is_bot(name, email):
                        seen[(name, email)] = seen.get((name, email), 0) + 1
            for name, email in TRAILER.findall(p["body"] or ""):
                if not is_bot(name, email):
                    seen[(name, email)] = len(commits)
            for (name, email), cnt in seen.items():
                if cnt * 2 < len(commits):  # a stacked PR that merely carries a few commits
                    continue
                l = resolve(name, email, repo) or name
                if l != login and l not in credited:
                    credited.append(l)
            for n in map(int, REFS.findall(p["body"] or "")):
                src = by_num.get(n)
                l = ((src or {}).get("author") or {}).get("login")
                if l and l != login and not is_bot(l) and l not in credited:
                    credited.append(l)
            if credited:
                notes.append(f"#{p['number']} by {login} -> credited {', '.join(credited)}")
        if not credited:
            if is_bot(login) or a.get("is_bot"):
                continue
            credited = [login]
        for l in credited:
            q = person(l)
            q["merged_prs"] += 1
            q["prs"].append(p["number"])
            q["dates"].append(p["mergedAt"])
    for i in issues:
        l = (i["author"] or {}).get("login", "")
        if not l or is_bot(l) or (i["author"] or {}).get("is_bot"):
            continue
        q = person(l)
        q["issues_filed"] += 1
        q["issues"].append(i["number"])
        q["dates"].append(i["createdAt"])
    rows = []
    for q in people.values():
        d = sorted(q.pop("dates"))
        q["score"] = W_PR * q["merged_prs"] + W_ISSUE * q["issues_filed"]
        q["first_contribution"], q["last_contribution"] = d[0], d[-1]
        q["pr_numbers"] = q.pop("prs"); q["issue_numbers"] = q.pop("issues")
        rows.append(q)
    rows.sort(key=lambda q: (-q["score"], -q["merged_prs"], q["first_contribution"], q["login"].lower()))
    for n, q in enumerate(rows, 1):
        q["rank"] = n
    out = {"repo": repo, "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
           "weights": {"merged_pr": W_PR, "issue_filed": W_ISSUE}, "contributors": rows}
    with open(o.out, "w") as f:
        json.dump(out, f, indent=2); f.write("\n")
    print("\n".join(notes), file=sys.stderr)
    print(f"wrote {o.out}: {len(rows)} contributors", file=sys.stderr)

if __name__ == "__main__":
    main()
