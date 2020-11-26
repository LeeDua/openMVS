#include "../../libs/MVS/Common.h"
#include "../../libs/MVS/Scene.h"
#include <boost/program_options.hpp>
#include <fstream>

using namespace MVS;


// D E F I N E S ///////////////////////////////////////////////////

#define APPNAME _T("ViewFilter")


// S T R U C T S ///////////////////////////////////////////////////
namespace ViewFilter
{
String strInputFileName;
String strOutputFileName;
String strFilterFileName;
unsigned nArchiveType;
int nProcessPriority;
unsigned nMaxThreads;
String strExportType;
boost::program_options::variables_map vm;
} // namespace ViewFilter


// initialize and parse the command line parameters
bool Initialize(size_t argc, LPCTSTR* argv)
{
	// initialize log and console
	OPEN_LOG();
	OPEN_LOGCONSOLE();

	// group of options allowed only on command line
	boost::program_options::options_description generic("Generic options");
	generic.add_options()
		("help,h", "produce this help message")
		("working-folder,w", boost::program_options::value<std::string>(&WORKING_FOLDER), "working directory (default current directory)")
		("export-type", boost::program_options::value<std::string>(&ViewFilter::strExportType)->default_value(_T("ply")), "file type used to export the 3D scene (ply or obj)")
		("archive-type", boost::program_options::value<unsigned>(&ViewFilter::nArchiveType)->default_value(2), "project archive type: 0-text, 1-binary, 2-compressed binary")
		("process-priority", boost::program_options::value<int>(&ViewFilter::nProcessPriority)->default_value(-1), "process priority (below normal by default)")
		("max-threads", boost::program_options::value<unsigned>(&ViewFilter::nMaxThreads)->default_value(0), "maximum number of threads (0 for using all available cores)")
		#if TD_VERBOSE != TD_VERBOSE_OFF
		("verbosity,v", boost::program_options::value<int>(&g_nVerbosityLevel)->default_value(
			#if TD_VERBOSE == TD_VERBOSE_DEBUG
			3
			#else
			2
			#endif
			), "verbosity level")
		#endif
		;

	// group of options allowed both on command line and in config file
	boost::program_options::options_description config("Viewfilter options");
	config.add_options()
		("input-file,i", boost::program_options::value<std::string>(&ViewFilter::strInputFileName), "input filename containing camera poses and image list")
		("input-filter-file,f", boost::program_options::value<std::string>(&ViewFilter::strFilterFileName), "input filter file name")
		("output-file,o", boost::program_options::value<std::string>(&ViewFilter::strOutputFileName), "output filename for storing the mesh")
		;
	
	boost::program_options::options_description cmdline_options;
	cmdline_options.add(generic).add(config);

	boost::program_options::positional_options_description p;
	p.add("input-file", -1);

	try {
		// parse command line options
		boost::program_options::store(boost::program_options::command_line_parser((int)argc, argv).options(cmdline_options).positional(p).run(), ViewFilter::vm);
		boost::program_options::notify(ViewFilter::vm);
		INIT_WORKING_FOLDER;
	}
	catch (const std::exception& e) {
		LOG(e.what());
		return false;
	}

	// initialize the log file
	OPEN_LOGFILE(MAKE_PATH(APPNAME _T("-")+Util::getUniqueName(0)+_T(".log")).c_str());

	// print application details: version and command line
	Util::LogBuild();
	LOG(_T("Command line:%s"), Util::CommandLineToString(argc, argv).c_str());

	// validate input
	Util::ensureValidPath(ViewFilter::strInputFileName);
	Util::ensureUnifySlash(ViewFilter::strInputFileName);
	if (ViewFilter::vm.count("help") || ViewFilter::strInputFileName.IsEmpty()) {
		boost::program_options::options_description visible("Available options");
		visible.add(generic).add(config);
		GET_LOG() << visible;
	}
	if (ViewFilter::strInputFileName.IsEmpty())
		return false;
	ViewFilter::strExportType = ViewFilter::strExportType.ToLower() == _T("obj") ? _T(".obj") : _T(".ply");

	// initialize optional options
	Util::ensureValidPath(ViewFilter::strOutputFileName);
	Util::ensureUnifySlash(ViewFilter::strOutputFileName);
	if (ViewFilter::strOutputFileName.IsEmpty())
		ViewFilter::strOutputFileName = Util::getFileFullName(ViewFilter::strInputFileName) + _T("_filtered.mvs");

	// initialize global options
	Process::setCurrentProcessPriority((Process::Priority)ViewFilter::nProcessPriority);
	#ifdef _USE_OPENMP
	if (ViewFilter::nMaxThreads != 0)
		omp_set_num_threads(ViewFilter::nMaxThreads);
	#endif

	#ifdef _USE_BREAKPAD
	// start memory dumper
	MiniDumper::Create(APPNAME, WORKING_FOLDER);
	#endif

	Util::Init();
	return true;
}

// finalize application instance
void Finalize()
{
	#if TD_VERBOSE != TD_VERBOSE_OFF
	// print memory statistics
	Util::LogMemoryInfo();
	#endif

	CLOSE_LOGFILE();
	CLOSE_LOGCONSOLE();
	CLOSE_LOG();
}

int main(int argc, LPCTSTR* argv)
{
	#ifdef _DEBUGINFO
	// set _crtBreakAlloc index to stop in <dbgheap.c> at allocation
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);// | _CRTDBG_CHECK_ALWAYS_DF);
	#endif

	if (!Initialize(argc, argv))
		return EXIT_FAILURE;

	Scene scene(ViewFilter::nMaxThreads);
	// load and refine the coarse mesh
	if (!scene.Load(MAKE_PATH_SAFE(ViewFilter::strInputFileName)))
		return EXIT_FAILURE;

	if (scene.mesh.IsEmpty()) {
		VERBOSE("error: empty initial mesh");
		return EXIT_FAILURE;
	}
	TD_TIMER_START();
	
	std::cout << "Mesh Refine starts without gpu" << std::endl;

	std::unordered_set<std::string> imgSet;
	std::unordered_map<std::string, uint32_t> imgMap;
	std::ifstream ifs;
	ifs.open(ViewFilter::strFilterFileName, std::ifstream::in);
	std::string line;
	std::cout << "Start reading filter file" << std::endl;
	while(getline(ifs, line)){
		if(line=="")continue;
		std::cout << line << std::endl;
		std::string basename = "";
		/*std::size_t start = line.find_last_of("/");
		std::size_t end = line.find(".");
		if (start!=std::string::npos && end!=std::string::npos){
			basename = line.substr(start+1, end-start-1);
			std::cout << "File name extracted" << std::endl;
		}*/
		
		std::size_t ptr = line.find(" ");
		if (ptr!=std::string::npos)
			basename = line.substr(0,ptr);
		std::cout << "File name extracted: " << basename << std::endl;
		imgSet.insert(basename);
	}


	std::cout << "Images in scene." << std::endl;
	ImageArr filtered_images;
	for(auto imgIter:scene.images){
		if(imgSet.find(imgIter.name)!=imgSet.end()){
			filtered_images.push_back(imgIter);
		}
	}
	for(uint32_t i=0;i<filtered_images.size();i++){
		imgMap.insert(filtered_images[i], i);
	}

	filtered_images.clear();

	for(auto imgIter:scene.images){
		//std::cout << imgIter.name << std::endl;
		if(imgSet.find(imgIter.name)!=imgSet.end()){
			ViewScoreArr neighbors(imgIter.neighbors);
			int count = neighbors.GetSize();
			for(auto pNeighbor=neighbors.begin();pNeighbor!=neighbors.end();pNeighbor++) {
				Image& neighImg = scene.images[pNeighbor->idx.ID];
				if(imgSet.find(neighImg.name)==imgSet.end()){
					neighbors.erase(pNeighbor);
					continue;
				}
				pNeighbor->idx.ID=imgMap[neighImg.name];
			}
			imgIter.neighbors = neighbors;
			if(imgIter.neighbors.GetSize()!=count){
				std::cout << "Remove illegal neighbors of image " << imgIter.neighbors.GetSize() - count << " " << imgIter.name << std::endl;
			}
			filtered_images.push_back(imgIter);
		}
	}
	scene.images.swap(filtered_images);

	std::cout << "Filtered Images in scene: "  << scene.images.size() << std::endl;

	for(auto imgIter:scene.images){
		ViewScoreArr neighbors(imgIter.neighbors);
		for(auto pNeighbor=neighbors.begin();pNeighbor!=neighbors.end();pNeighbor++) {		
			assert(pNeighbor->idx.ID<scene.images.size());			
		}
	}

	
	VERBOSE("Mesh refinement completed: %u vertices, %u faces (%s)", scene.mesh.vertices.GetSize(), scene.mesh.faces.GetSize(), TD_TIMER_GET_FMT().c_str());

	// save the final mesh
	const String baseFileName(MAKE_PATH_SAFE(Util::getFileFullName(ViewFilter::strOutputFileName)));
	scene.Save(baseFileName+_T(".mvs"), (ARCHIVE_TYPE)ViewFilter::nArchiveType);
	scene.mesh.Save(baseFileName+ViewFilter::strExportType);
	#if TD_VERBOSE != TD_VERBOSE_OFF
	if (VERBOSITY_LEVEL > 2)
		scene.ExportCamerasMLP(baseFileName+_T(".mlp"), baseFileName+ViewFilter::strExportType);
	#endif

	Finalize();
	return EXIT_SUCCESS;
}
/*----------------------------------------------------------------*/
