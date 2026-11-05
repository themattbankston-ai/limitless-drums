#!/usr/bin/env python3
"""Builds harness.html: Hitline 4.8 exactly as shipped, plus one line at the end of its app
script that exposes its state and prep functions to the parity runner. No Hitline logic changes."""
import pathlib
here = pathlib.Path(__file__).parent
s = (here / 'hitline-4.8.html').read_text()
i = s.rfind("})();\n</script>")
assert i > 0, 'end of the app script not found'
hook = "window.__HL = { get S(){ return S; }, get P(){ return P; }, addSessionsFromFiles, autoPrep, runDetect, computeAll };\n"
(here / 'harness.html').write_text(s[:i] + hook + s[i:])
print('wrote harness.html')
