OPTION(block,1,0,1,"enable blocked clause elimination (BCE)")
OPTION(blkwait,1,0,INT_MAX,"wait with blocked clauses")
OPTION(blkrtc,0,0,INT_MAX,"run BCE until completion")
OPTION(boost,10,1,INT_MAX,"boost initial effor for simplification")
OPTION(bwclslim,10000,0,INT_MAX,"bw subsumption clause size limit")
OPTION(bwocclim,10000,0,INT_MAX,"bw subsumption occurrence limit")
OPTION(cbump,1,0,1,"clause bumping (0=none,1=inc,2=lru,3=avg)")
#ifndef NLGL
OPTION(check,0,0,1,"enable checking")
#endif
OPTION(distill,1,0,1,"enable distillation")
OPTION(elim,1,0,1,"enable bounded variable elimination (BVE)")
OPTION(elmrtc,0,0,INT_MAX,"run BVE until completion")
OPTION(elmocclim,10000,3,INT_MAX,"max occurrences in BVE")
OPTION(elmpocclim1,100,0,INT_MAX,"max one-sided occs of BVE pivot")
OPTION(elmpocclim2,8,0,INT_MAX,"max two-sided occs of BVE pivot")
OPTION(elmclslim,1000,3,INT_MAX,"max antecedent size in BVE")
OPTION(gluered,1,0,1,"enable glue (LBD) based reduction")
OPTION(gluekeep,1,0,INT_MAX,"keep clauses of this glue (LBD)")
OPTION(glueupdate,0,0,1,"update glues (LBDs)")
OPTION(itsimpdel,10,0,10000,"iteration simp delay in conflicts")
OPTION(plain,0,0,1,"plain solving (no inprocessing/preprocessing)")
OPTION(restart,1,0,1,"enable restart")
OPTION(restartint,100,1,INT_MAX,"basic restart interval")
OPTION(redinit,1000,1,INT_MAX,"initial reduction interval")
OPTION(redinc,2000,1,INT_MAX,"reduction interval increment")
OPTION(reusetrail,1,0,1,"reuse the trail")
OPTION(simpint,2000000,1,INT_MAX,"inprocessing steps interval")
OPTION(simpgeom,1,0,1,"geometric increase in simplification effort too")
OPTION(sizepen,(1<<17),1,INT_MAX,"size penalty number of clauses")
OPTION(sizemaxpen,5,0,20,"maximum logarithmic size penalty")
OPTION(searchint,5000,1,INT_MAX,"CDCL search conflict interval")
OPTION(searchfirst,0,0,1,"search first insetad of simplifying first")
OPTION(scincfact,1050,1000,10000,"variable score increment in per mille")
OPTION(stepslim,100000000,0,INT_MAX,"maximum steps limit (0=none))")
#ifndef NLOG
OPTION(log,0,0,1,"enable logging")
#endif
OPTION(verbose,0,0,1,"enable verbose messages")
OPTION(witness,1,0,1,"print witness")
