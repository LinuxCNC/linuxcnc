# Helper Scripts

## distribute-python

This script must be run whenever the protobuf files are changed. To
prevent problems when protoc is not installed on a system it makes
sense to dristribute the generated Python files.

Run this script from the root directory.

```bash
./script/distribute-python
```
