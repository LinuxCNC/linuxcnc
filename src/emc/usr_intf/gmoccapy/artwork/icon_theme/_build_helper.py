import os
import pathlib
from PIL import Image
from shlex import join
from functools import reduce

class Dimension:
    def __init__(self, size, scale=None):
        self.size = size
        self.scale = scale

    def to_string(self):
        s = f"{self.size}x{self.size}"
        return s if not(self.scale) else s + f"@{self.scale}"

class Composer:

    class _Composition:

        def __init__(self, name, src, layers=None, context=None):
            self.name = name
            self.src = src
            self.layers = layers
            self.context = context

        def _is_export_required(self, src_file, out_file):
            if not out_file.exists():
                return True
            return os.path.getmtime(src_file) > os.path.getmtime(out_file)

        def _export_layer(self, temp_dir, src_file, dimension, layer=None):
            out_file = pathlib.Path(temp_dir).joinpath(f"{src_file.stem}.{layer + '-' if layer else ''}{dimension.to_string()}.png")
            if self._is_export_required(src_file, out_file):
                size = dimension.size * (dimension.scale or 1)
                command = ["inkscape", "-o", str(out_file)]
                if layer:
                    command += ["-i", layer, "-j"]
                command += ["-C", "-w", str(size), "-h", str(size), str(src_file)]
                os.system(join(command))
            return Image.open(out_file)

        def export(self, target_dir, temp_dir, dimensions):
            os.makedirs(temp_dir, exist_ok=True)
            src_file = pathlib.Path(self.src)

            for dimension in dimensions:
                target_file = pathlib.Path(target_dir).joinpath(dimension.to_string(), self.context or "", self.name)
                if self.layers:
                    images = list([self._export_layer(temp_dir, src_file, dimension, layer) for layer in self.layers])
                    target_file.parent.mkdir(parents=True, exist_ok=True)
                    reduce(lambda a, b: Image.alpha_composite(a, b), images).save(target_file)
                    print(f"Composed image {target_file}")
                else:
                    self._export_layer(target_file, src_file, dimension)
                    if target_file.exists():
                        print(f"Rendered image {target_file}")
                    else:
                        raise Exception(f"Failed to render image: inkscape created no file at {target_file}")

    def __init__(self, target_dir, temp_dir):
        self.target_dir = target_dir
        self.temp_dir = temp_dir
        self.compositions = []

    def add(self, out_file, src_file, context=None, layers=None):
        self.compositions.append(self._Composition(out_file, src_file, layers, context))

    def compose(self, dimensions):
        os.makedirs(self.temp_dir, exist_ok=True)

        for comp in self.compositions:
            comp.export(self.target_dir, self.temp_dir, dimensions)
