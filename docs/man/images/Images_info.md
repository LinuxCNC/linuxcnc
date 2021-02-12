# Images in man pages

Images can be added in the man pages for representation as HTML and PDF.
The images have to be in *.ps-format and should be placed only in this folder.
They can be included using `.PSPIC -L ../man/images/image.ps`.

The PNG needed for the HTML view is automatically created on generating LinuxCNC_Manual_Pages.pdf


## Examples

Here are some examples for creating waveforms very easily using [Wavedrom](https://wavedrom.com/editor.html).
Export it as SVG (button in the lower right corner) an then convert it to postscript using Inkscape for example.

toggle2nist:

```
{'signal': [
  {name: 'in', wave:  'lHx.l.Hx.l'},
  {name: 'on', wave: 'lh.l......' }, 
  {name: 'off', wave: 'l.....h.l.' }, 
  {name: 'is-on', wave: 'l..h....l.' }, 
]}
```

toggle:

```
{signal: [
  {name: 'in', wave:  'lPlP'},
  {name: 'out', wave: 'lh.l' }, 
]}
```
