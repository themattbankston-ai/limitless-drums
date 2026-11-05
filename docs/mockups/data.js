const SR = 44100;
const MICS = [
 { mic:'Bottom', role:'bottom', col:['#4c8dff','#2ee6ff'], lagS:80.975, arrMs:1.84, pol:1, ap:'202 Hz', kept:false, applied:true,
   before:[0.37,0.26,0.62,0.15,0.08,-0.01,-0.05,-0.01], after:[0.58,0.46,0.87,-0.05,0,-0.04,0.02,0], asis:0.351, score:0.477 },
 { mic:'Overheads', role:'near', col:['#4c8dff','#8b5cff'], lagS:22.725, arrMs:0.52, pol:-1, ap:null, kept:false, applied:true,
   before:[0.7,-0.63,-0.67,0.24,0.01,0.11,-0.09,-0.02], after:[-0.71,0.51,0.85,0.23,-0.1,-0.13,0.02,0], asis:-0.313, score:0.409 },
 { mic:'Room Mono', role:'far', col:['#8b5cff','#ff3da8'], lagS:95.975, arrMs:0.67, pol:1, ap:null, kept:true, applied:false,
   before:[-0.29,0.64,0.6,-0.19,-0.07,0,-0.01,0], after:[0.55,0.53,0.57,0.3,-0.01,0,-0.01,0.05], asis:0.303, score:0.384 },
 { mic:'Room Close', role:'far', col:['#8b5cff','#ff3da8'], lagS:639.475, arrMs:12.55, pol:1, ap:null, kept:true, applied:false,
   before:[-0.23,0.12,-0.2,0.08,0,0,0.02,-0.01], after:[0.32,0.23,0.3,0.34,-0.02,0.06,0.02,0.01], asis:0.005, score:0.048 },
 { mic:'Room Crush', role:'far', col:['#ff3da8','#ff7ad9'], lagS:627.975, arrMs:12.27, pol:1, ap:null, kept:true, applied:false,
   before:[0.03,-0.03,-0.05,0.04,0.01,-0.02,0,0.01], after:[-0.3,-0.15,0.21,0.31,0.04,0.05,-0.01,-0.02], asis:0.001, score:0.101 },
 { mic:'Room Far', role:'far', col:['#8b5cff','#2ee6ff'], lagS:1253.975, arrMs:26.01, pol:-1, ap:null, kept:true, applied:false,
   before:[-0.06,-0.06,-0.1,0.03,0.02,0.01,0.01,0.01], after:[-0.04,0.06,0.39,0.2,-0.08,0.05,-0.03,0.01], asis:-0.051, score:0.244 },
];
const BANDS = ['45','90','180','355','710','1.4k','2.8k','5.6k','11k'];
