# VAML — SESSION-CONTEXT (per-run state)

- last_command    : **S163 COMPLETE (working-tree; no commits) — Best-GPU auto-selection.** A Cat 1
                    (library-surface) vaml::vulkan::gpu capability + the S162-recorded loader expansion
                    + both POCs migrated. (1) LOADER: VulkanLoader binds
                    vkGetPhysicalDeviceMemoryProperties (device-local memory scoring) + the S163 device
                    entries vkCreateDevice/vkGetDeviceQueue/vkDestroyDevice (previously owned by the
                    DELETED pre-S162 VamlPOC/VulkanLoader.h — NO-deadwood); has_device_loader() now true
                    when vkCreateDevice+vkGetDeviceQueue are bound. (2) PhysicalDeviceInfo (S162) gains
                    deviceLocalBytes (sum of VK_MEMORY_HEAP_DEVICE_LOCAL_BIT heaps in
                    enumerate_physical_devices). (3) NEW gpu/BestGpuSelection.h/.cpp: PURE seams
                    device_type_rank (DISCRETE=2>INTEGRATED=1>OTHER=0) + better_device (type DESC,
                    deviceLocalBytes DESC, apiVersion DESC — strict weak order) + rank_physical_devices
                    (stable, best-first) + preferred_gpu_env_name (VAML_PREFERRED_GPU, MSVC-C4996-safe);
                    REAL select_best_gpu(loader, instance, surfaceOpt, preferredGpuName) — scores ALL
                    devices, filters to present-capable families (real
                    vkGetPhysicalDeviceSurfaceSupportKHR per family; always-present when no surface —
                    headless/dev), applies the VAML_PREFERRED_GPU name override (force-rank-first),
                    sorts + logs the FULL ranked list at init (rank/name/type/device-local GiB/
                    apiVersion/present), picks the best present-capable device, robust fallback to
                    top-of-rank when none present-capable. (4) BOTH POCs migrated (identical code = ONE
                    select_best_gpu call, not two copies): VamlPOC.cpp (Windows) + VamlPOC-linux/main.cpp
                    each create VulkanLoader→VulkanInstance→VulkanSurface, call the SAME gpu::
                    select_best_gpu, read VAML_PREFERRED_GPU, build the logical device+queue via the new
                    loader device entries; the hand-rolled instance/GPU loops are GONE. (5) POC BUILD:
                    Windows VamlPOC.exe (VS 2026 msbuild Debug/x64) clean; Linux vamlpoc-linux (VM clang)
                    clean. LIVE (manual, [both apps, not CI]): Linux VM (Mesa, single llvmpipe CPU
                    device) RAN the POC on the Xwayland :0 display — the S163 ranked log printed (#1
                    llvmpipe, CPU/OTHER tier, 3.27 GiB, api 1.4.335, present), the device selected, the
                    window opened + ran the loop (timeout 124 = running); the VAML_PREFERRED_GPU override
                    path exercised with a name-match. The two-GPU Windows multi-adapter live proof
                    requires a real two-GPU desktop host (recorded NO-GAP; the comparator is proven
                    headless by BestGpu_test.cpp on both CI lanes). No commits.
- plan_repo_state : **S160 + S161 + S162 + S163 COMPLETE — Sprint 4 window + Vulkan/VMA linkage + S003
                    vulkan marker resolved + Vulkan instance/surface/device/queue-family surface + Best-GPU
                    auto-selection.** CHECKPOINT (progress.md) still reads last = S147 / next = S160 —
                    progress.md was NOT updated this sprint (roadmap + session-context only; progress.md
                    SURVIVOR is the next wrap step's job, matching the S139/S147 docs-only-wrap
                    precedent). Roadmap: S160 [DONE] + S161 [DONE] + S162 [DONE] + S163 [DONE]; S164
                    (swapchain) is the next unimplemented token. No commits. Deletions-pending (from the
                    prior dep-migration) + the dep-migration commit plan remain outstanding (see notes).
- plan_digest     : S163 (dep:S162) = MOD src/vulkan/CMakeLists.txt (+BestGpuSelection.cpp) + MOD
                    src/vulkan/src/{VulkanLoader.h,cpp} (S163 device entries + vkGetPhysicalDeviceMemory-
                    Properties) + MOD src/vulkan/include/vaml/vulkan/QueueFamilySelection.h
                    (+deviceLocalBytes) + ADDED src/vulkan/include/vaml/vulkan/gpu/BestGpuSelection.h +
                    ADDED src/vulkan/src/BestGpuSelection.cpp + ADDED test/unit/BestGpu_test.cpp + MOD
                    test/unit/CMakeLists.txt (+BestGpu_test.cpp) + ADDED build/verify_s163.cmd + MOD
                    VamlPOC/VamlPOC.cpp + MOD VamlPOC/VamlPOC.vcxproj (DEL local VulkanLoader.h) + MOD
                    VamlPOC-linux/src/main.cpp + MOD docs/roadmap.md. Prior: S160/S161/S162 + the
                    dep-migration (see prior session-context). Both POCs route window through
                    PlatformWindow AND GPU through gpu::select_best_gpu.
- files_touched   :
    ADDED     src/vulkan/include/vaml/vulkan/gpu/BestGpuSelection.h   (S163 Cat-1 surface: ScoredDevice + device_type_rank + better_device + rank_physical_devices + BestGpuResult + select_best_gpu + preferred_gpu_env_name)
    ADDED     src/vulkan/src/BestGpuSelection.cpp                     (S163 implementation: ranking + present-capable filter + VAML_PREFERRED_GPU override + full ranked log + selection)
    MODIFIED  src/vulkan/src/VulkanLoader.h                           (+vkGetPhysicalDeviceMemoryProperties + vkCreateDevice/vkGetDeviceQueue/vkDestroyDevice; has_device_loader updated)
    MODIFIED  src/vulkan/src/VulkanLoader.cpp                         (resolve the 4 new entry points)
    MODIFIED  src/vulkan/include/vaml/vulkan/QueueFamilySelection.h   (+PhysicalDeviceInfo::deviceLocalBytes)
    MODIFIED  src/vulkan/src/QueueFamilySelection.cpp                 (populate deviceLocalBytes from DEVICE_LOCAL heaps)
    MODIFIED  src/vulkan/CMakeLists.txt                               (+src/BestGpuSelection.cpp)
    ADDED     test/unit/BestGpu_test.cpp                              (S163 headless ranking seams + env override test)
    MODIFIED  test/unit/CMakeLists.txt                                (+BestGpu_test.cpp into vaml-vulkan-tests)
    ADDED     build/verify_s163.cmd
    MODIFIED  VamlPOC/VamlPOC.cpp                                     (migrated onto VulkanInstance/VulkanSurface/gpu::select_best_gpu + S163 device entries; deleted local loader usage)
    MODIFIED  VamlPOC/VamlPOC.vcxproj                                 (DEL local VulkanLoader.h from ClInclude)
    MODIFIED  VamlPOC-linux/src/main.cpp                              (migrated onto the same library surface)
    DELETED   VamlPOC/VulkanLoader.h                                  (pre-S162 local loader — NO-deadwood, superseded by the library VulkanLoader)
    MODIFIED  docs/roadmap.md                                         (S163 DONE)
    MODIFIED  session-context.md (this)
    (prior S160/S161/S162 + dep-migration files unchanged)
- build_status    : **GREEN — BOTH CI LANES VERIFIED FROM FIRST PRINCIPLES.** windows-msvc
                    (build/verify_s163.cmd, msvc-debug): configure clean + build warning-as-error clean
                    + ctest 2/2 PASS (vaml-core-tests + vaml-vulkan-tests — 31 S163 test cases incl.
                    BestGpu_test). ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug on the VM, shared-folder
                    mirror): configure clean + build warning-clean (only the pre-existing upstream
                    <ciso646> doctest deprecation, non-error) + ctest 2/2 PASS. windows-clang-debug is
                    NOT a lane (pre-existing /Zc:preprocessor failure; not built/tested/flagged per
                    standing rule). POC BUILD: Windows VamlPOC.exe (VS 2026 msbuild Debug/x64) clean;
                    Linux vamlpoc-linux (VM, VamlPOC-linux/build-ninja rerun against the fresh
                    clang-debug libs) clean. LIVE (manual, [both apps, not CI]): Linux VM ran the POC on
                    the Xwayland :0 display — ranked log printed + device selected + window loop ran
                    (timeout 124); VAML_PREFERRED_GPU override exercised.
- next_step       : **COMMIT PLAN** (outstanding — no commits yet, per standing directive): the
                    cumulative working-tree changes (dep-migration removal + S160/S161/S162/S163) remain
                    uncommitted. The commit-plan ordering (create+push PUBLIC vaml-deps on GitHub FIRST,
                    then commit the dep removal + rewires + CI bootstrap, then VamlPOC) is recorded to
                    avoid CI fetch-timing failures. THEN next roadmap token: **S164 (dep:S163)**
                    "Swapchain (VK_KHR_swapchain) + image views + depth; format/BPP negotiation" —
                    consumes select_best_gpu's chosen PhysicalDeviceInfo + the S163-bound device entries
                    (vkCreateDevice/vkGetDeviceQueue/vkDestroyDevice) + VulkanSurface. NO-GAP to carry:
                    WindowCallbacks->routed events S323/S324; resize-safe swapchain S167; VMA consumed
                    S163/S202; best-GPU two-GPU multi-adapter live proof = a desktop host (manual, not
                    CI, recorded).
- notes           :
    - S163 categorization (user-confirmed at plan time): Best-GPU *selection* is Cat 1 (library
      surface — every future consumer chooses a GPU), authored under the namespace-suggestive
      vaml::vulkan::gpu:: and permanent; the *wiring* (window + loop + device consumption) is Cat 2
      (app-owned). "Identical code in both apps" = the SAME select_best_gpu call, not a duplicated
      algorithm. This satisfies the roadmap title without drift-prone copies (NO-LAZY-LLM/DRY).
    - The VamlPOC-local pre-S162 VulkanLoader.h is DELETED (superseded by the library VulkanLoader,
      which now also binds the S163 device entries — the recorded S162 "expanded by later sprints"
      resolution; S163 is the first sprint to create a device on the chosen GPU). NO-deadwood.
    - select_best_gpu present-capable filter uses a REAL vkGetPhysicalDeviceSurfaceSupportKHR probe per
      queue family when a VulkanSurface is supplied; with no surface (headless/dev) every device is
      present-capable (always-present predicate) — documented in the header, not silent.
    - VAML_PREFERRED_GPU override is read via preferred_gpu_env_name() (MSVC _dupenv_s to avoid C4996);
      a name-matching device is force-ranked first (explicit user force over the auto-score).
    - The two-GPU Windows multi-adapter selection is a DESKTOP-HOST manual proof ([both apps, not CI] —
      the Linux VM exposes only llvmpipe, a single CPU device). The ranking comparator (DISCRETE>
      INTEGRATED>OTHER then memory then apiVersion) is proven headless by BestGpu_test.cpp on both CI
      lanes; the single-GPU live ranking+override path is proven on the VM.
    - COMMITIT STATUS: cumulative work through S163 + the dep-migration is outstanding (dep-migration +
      S147 + S160 + S161 + S162 + S163). No commits made (per command). The commit-plan ordering
      (vaml-deps public + push-before-VamlPOC) is recorded to avoid CI fetch-timing failures.
