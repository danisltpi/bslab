# Labor Betriebssysteme
## Filesystem in FUSE


### Aufgabenstellung
Ziel des Labors ist die Implementierung eines Filesystems in FUSE.
Dabei ist eine Implementierung persistent und eine andere nur in RAM,
d.h. die Datein sind nach dem schliessen des Dateisystems nicht mehr 
aufzurufen.

### Lösungsansatz und Umsetzung
Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.


### Tests
```cpp
int MyOnDiskFS::fuseTruncate(const char *path, off_t newSize);
```
