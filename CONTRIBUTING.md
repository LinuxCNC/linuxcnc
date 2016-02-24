To improve tracking of who did what, we use the "sign-off" procedure introduced by the Linux kernel. The sign-off is a simple line at the end of the explanation for the patch, which certifies that you wrote it or otherwise have the right to pass it on as an open-source patch. The rules are pretty simple: if you can certify the Developer’s Certificate of Origin 1.1 with GPLv2+ clause, then you just add a line saying

~~~~
Signed-off-by: Random J Developer <random@developer.example.org>
~~~~

(you must use your real name and a working e-mail address)

This line can be automatically added by git if you run the git-commit command with the -s option.

For more information, see docs/SubmittingPatches and docs/developer-certificate-of-origin in the source tree. By policy, commits authored after 2014-09-29 must bear a signed-off-by line or they will be rejected.
