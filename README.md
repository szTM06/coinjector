# coinjector
Inject DLL at same address as existing DLL whilst retaining full functionality (sort of, process affinity has to be set to single core ):) via PFN modification on thread switches
(certain threads can access the new DLL while remaining threads access the old one)

Usage - um <exe to inject into> <dll_to_replace> <dll_to_inject>
