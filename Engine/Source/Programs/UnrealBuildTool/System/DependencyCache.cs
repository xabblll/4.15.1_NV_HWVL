// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;

namespace UnrealBuildTool
{
	[DebuggerDisplay("{IncludeName}")]
	public class DependencyInclude
	{
		/// <summary>
		/// These are direct include paths and cannot be resolved to an actual file on disk without using the proper list of include directories for this file's module 
		/// </summary>
		public readonly string IncludeName;

		/// <summary>
		/// Whether we've attempted to resolve this include (may be set even if IncludeResolvedNameIfSuccessful = null, in cases where it couldn't be found)
		/// </summary>
		public bool HasAttemptedResolve;

		/// <summary>
		/// This is the fully resolved name, and a bool for whether we've attempted the resolve but failed. We can't really store this globally, but we're going to see how well it works.
		/// </summary>
		public FileReference IncludeResolvedNameIfSuccessful;

		/// <summary>
		/// Public ctor that initializes the include name (the resolved name won't be determined until later)
		/// </summary>
		/// <param name="InIncludeName"></param>
		public DependencyInclude(string InIncludeName)
		{
			IncludeName = InIncludeName;
		}
	}

	/**
	 * Caches include dependency information to speed up preprocessing on subsequent runs.
	 */
	public class DependencyCache
	{
		/** The version number for binary serialization */
		const int FileVersion = 1;

		/** The file signature for binary serialization */
		const int FileSignature = ('D' << 24) | ('C' << 16) | FileVersion;

		/** Path to store the cache data to. */
		private FileReference BackingFile;

		/** The time the cache was created. Used to invalidate entries. */
		public DateTime CreationTimeUtc;

		/** The time the cache was last updated. Stored as the creation date when saved. Not serialized. */
		private DateTime UpdateTimeUtc;

		/** Dependency lists, keyed on file's absolute path. */
		private Dictionary<FileReference, List<DependencyInclude>> DependencyMap;

		/** A mapping of whether various files exist. Not serialized. */
		private Dictionary<FileReference, bool> FileExistsInfo;

		/** Whether the dependency cache is dirty and needs to be saved. Not serialized. */
		private bool bIsDirty;

		/**
		 * Creates and deserializes the dependency cache at the passed in location
		 * 
		 * @param	CachePath	Name of the cache file to deserialize
		 */
		public static DependencyCache Create(FileReference CacheFile)
		{
			// See whether the cache file exists.
			if (CacheFile.Exists())
			{
				if (BuildConfiguration.bPrintPerformanceInfo)
				{
					Log.TraceInformation("Loading existing IncludeFileCache: " + CacheFile.FullName);
				}

				var TimerStartTime = DateTime.UtcNow;

				// Deserialize cache from disk if there is one.
				DependencyCache Result = Load(CacheFile);
				if(Result != null)
				{
					// Successfully serialize, create the transient variables and return cache.
					Result.UpdateTimeUtc = DateTime.UtcNow;

					var TimerDuration = DateTime.UtcNow - TimerStartTime;
					if (BuildConfiguration.bPrintPerformanceInfo)
					{
						Log.TraceInformation("Loading IncludeFileCache took " + TimerDuration.TotalSeconds + "s");
					}
					//Telemetry.SendEvent("LoadIncludeDependencyCacheStats.2", "TotalDuration", TimerDuration.TotalSeconds.ToString("0.00"));
					return Result;
				}
			}
			// Fall back to a clean cache on error or non-existance.
			return new DependencyCache(CacheFile);
		}

		/**
		 * Loads the cache from the passed in file.
		 * 
		 * @param	Cache	File to deserialize from
		 */
		public static DependencyCache Load(FileReference CacheFile)
		{
			DependencyCache Result = null;
			try
			{
				string CacheBuildMutexPath = CacheFile.FullName + ".buildmutex";

				// If the .buildmutex file for the cache is present, it means that something went wrong between loading
				// and saving the cache last time (most likely the UBT process being terminated), so we don't want to load
				// it.
				if (!File.Exists(CacheBuildMutexPath))
				{
					using (File.Create(CacheBuildMutexPath))
					{
					}

					using (BinaryReader Reader = new BinaryReader(new FileStream(CacheFile.FullName, FileMode.Open, FileAccess.Read)))
					{
						if(Reader.ReadInt32() == FileSignature)
						{
							Result = DependencyCache.Deserialize(Reader);
						}
					}
				}
			}
			catch (Exception Ex)
			{
				Console.Error.WriteLine("Failed to read dependency cache: {0}", Ex.Message);
			}
			return Result;
		}

		/// <summary>
		/// Serializes the dependency cache to a binary writer
		/// </summary>
		/// <param name="Writer">Writer to output to</param>
		void Serialize(BinaryWriter Writer)
		{
			Writer.Write(BackingFile);
			Writer.Write(CreationTimeUtc.ToBinary());

			Dictionary<FileReference, int> FileToUniqueId = new Dictionary<FileReference,int>();

			Writer.Write(DependencyMap.Count);
			foreach(KeyValuePair<FileReference, List<DependencyInclude>> Pair in DependencyMap)
			{
				Writer.Write(Pair.Key);
				Writer.Write(Pair.Value.Count);
				foreach(DependencyInclude Include in Pair.Value)
				{
					Writer.Write(Include.IncludeName);
					Writer.Write(Include.HasAttemptedResolve);
					Writer.Write(Include.IncludeResolvedNameIfSuccessful, FileToUniqueId);
				}
			}
		}

		/// <summary>
		/// Deserialize the dependency cache from a binary reader
		/// </summary>
		/// <param name="Reader">Reader for the cache data</param>
		/// <returns>New dependency cache object</returns>
		static DependencyCache Deserialize(BinaryReader Reader)
		{
			DependencyCache Cache = new DependencyCache(Reader.ReadFileReference());
			Cache.CreationTimeUtc = DateTime.FromBinary(Reader.ReadInt64());

			int NumEntries = Reader.ReadInt32();
			Cache.DependencyMap = new Dictionary<FileReference,List<DependencyInclude>>(NumEntries);

			List<FileReference> UniqueFiles = new List<FileReference>();
			for(int Idx = 0; Idx < NumEntries; Idx++)
			{
				FileReference File = Reader.ReadFileReference();

				int NumIncludes = Reader.ReadInt32();
				List<DependencyInclude> Includes = new List<DependencyInclude>(NumIncludes);

				for(int IncludeIdx = 0; IncludeIdx < NumIncludes; IncludeIdx++)
				{
					DependencyInclude Include = new DependencyInclude(Reader.ReadString());
					Include.HasAttemptedResolve = Reader.ReadBoolean();
					Include.IncludeResolvedNameIfSuccessful = Reader.ReadFileReference(UniqueFiles);
					Includes.Add(Include);
				}

				Cache.DependencyMap.Add(File, Includes);
			}

			Cache.CreateFileExistsInfo();
			Cache.ResetUnresolvedDependencies();
			return Cache;
		}

		/**
		 * Constructor
		 * 
		 * @param	Cache	File associated with this cache
		 */
		protected DependencyCache(FileReference InBackingFile)
		{
			BackingFile = InBackingFile;
			CreationTimeUtc = DateTime.UtcNow;
			UpdateTimeUtc = DateTime.UtcNow;
			DependencyMap = new Dictionary<FileReference, List<DependencyInclude>>();
			bIsDirty = false;
			CreateFileExistsInfo();
		}

		/**
		 * Saves the dependency cache to disk using the update time as the creation time.
		 */
		public void Save()
		{
			// Only save if we've made changes to it since load.
			if (bIsDirty)
			{
				var TimerStartTime = DateTime.UtcNow;

				// Save update date as new creation date.
				CreationTimeUtc = UpdateTimeUtc;

				// Serialize the cache to disk.
				try
				{
					BackingFile.Directory.CreateDirectory();
					using(BinaryWriter Writer = new BinaryWriter(new FileStream(BackingFile.FullName, FileMode.Create, FileAccess.Write)))
					{
						Writer.Write(FileSignature);
						Serialize(Writer);
					}
				}
				catch (Exception Ex)
				{
					Console.Error.WriteLine("Failed to write dependency cache: {0}", Ex.Message);
				}

				if (BuildConfiguration.bPrintPerformanceInfo)
				{
					var TimerDuration = DateTime.UtcNow - TimerStartTime;
					Log.TraceInformation("Saving IncludeFileCache took " + TimerDuration.TotalSeconds + "s");
				}
			}
			else
			{
				if (BuildConfiguration.bPrintPerformanceInfo)
				{
					Log.TraceInformation("IncludeFileCache did not need to be saved (bIsDirty=false)");
				}
			}

			FileReference MutexPath = BackingFile + ".buildmutex";
			if(MutexPath.Exists())
			{
				try
				{
					MutexPath.Delete();
				}
				catch
				{
					// We don't care if we couldn't delete this file, as maybe it couldn't have been created in the first place.
				}
			}
		}

		/**
		 * Returns the direct dependencies of the specified FileItem if it exists in the cache and they are not stale.
		 *
		 * @param  File  File to try to find dependencies in cache
		 * @returns  Dependency info for File, or null if no dependencies are cached or if the cache is stale.
		 */
		public List<DependencyInclude> GetCachedDependencyInfo(FileItem File)
		{
			// Check whether File is in cache.
			List<DependencyInclude> Includes;
			if (!DependencyMap.TryGetValue(File.Reference, out Includes))
			{
				return null;
			}

			// File is in cache, now check whether last write time is prior to cache creation time.
			if (File.LastWriteTime.ToUniversalTime() >= CreationTimeUtc)
			{
				// Remove entry from cache as it's stale.
				DependencyMap.Remove(File.Reference);
				bIsDirty = true;
				return null;
			}

			// Check if any of the resolved includes is missing
			foreach (var Include in Includes)
			{
				if (Include.IncludeResolvedNameIfSuccessful != null)
				{
					bool bIncludeExists = false;
					if (!FileExistsInfo.TryGetValue(Include.IncludeResolvedNameIfSuccessful, out bIncludeExists))
					{
						bIncludeExists = Include.IncludeResolvedNameIfSuccessful.Exists();
						FileExistsInfo.Add(Include.IncludeResolvedNameIfSuccessful, bIncludeExists);
					}

					if (!bIncludeExists)
					{
						// Remove entry from cache as it's stale, as well as the include which no longer exists
						DependencyMap.Remove(Include.IncludeResolvedNameIfSuccessful);
						DependencyMap.Remove(File.Reference);
						bIsDirty = true;
						return null;
					}
				}
			}

			// Cached version is up to date, return it.
			return Includes;
		}

		/**
		 * Update cache with dependencies for the passed in file.
		 * 
		 * @param	File			File to update dependencies for
		 * @param	Dependencies	List of dependencies to cache for passed in file
		 * @param	HasUObjects		True if this file was found to contain UObject classes or types
		 */
		public void SetDependencyInfo(FileItem File, List<DependencyInclude> Info)
		{
			DependencyMap[File.Reference] = Info;
			bIsDirty = true;
		}

		/// <summary>
		/// Creates and pre-allocates a map for storing information about the physical presence of files on disk.
		/// </summary>
		private void CreateFileExistsInfo()
		{
			// Pre-allocate about 125% of the dependency map count (which amounts to 1.25 unique includes per dependency which is a little more than empirical
			// results show but gives us some slack in case something gets added).
			FileExistsInfo = new Dictionary<FileReference, bool>((DependencyMap.Count * 5) / 4);
		}

		/// <summary>
		/// Resets unresolved dependency include files so that the compile environment can attempt to re-resolve them.
		/// </summary>
		public void ResetUnresolvedDependencies()
		{
			foreach (var Dependency in DependencyMap)
			{
				foreach (var Include in Dependency.Value)
				{
					if (Include.HasAttemptedResolve && Include.IncludeResolvedNameIfSuccessful == null)
					{
						Include.HasAttemptedResolve = false;
					}
				}
			}
		}

		/// <summary>
		/// Caches the fully resolved path of the include.
		/// TODO: This method should be more tightly coupled with the Resolve step itself so we don't have to reach into the cache externally
		/// using internal details like the list index.
		/// </summary>
		/// <param name="File">The file whose include is being resolved</param>
		/// <param name="DirectlyIncludedFileNameIndex">Index in the resolve list to quickly find the include in question in the existing cache.</param>
		/// <param name="DirectlyIncludedFileNameFullPath">Full path name of the resolve include.</param>
		public void CacheResolvedIncludeFullPath(FileItem File, int DirectlyIncludedFileNameIndex, FileReference DirectlyIncludedFileNameFullPath)
		{
			if (BuildConfiguration.bUseIncludeDependencyResolveCache)
			{
				var Includes = DependencyMap[File.Reference];
				var IncludeToResolve = Includes[DirectlyIncludedFileNameIndex];
				if (BuildConfiguration.bTestIncludeDependencyResolveCache)
				{
					// test whether there are resolve conflicts between modules with different include paths.
					if (IncludeToResolve.HasAttemptedResolve && IncludeToResolve.IncludeResolvedNameIfSuccessful != DirectlyIncludedFileNameFullPath)
					{
						throw new BuildException("Found directly included file that resolved differently in different modules. File ({0}) had previously resolved to ({1}) and now resolves to ({2}).",
							File.AbsolutePath, IncludeToResolve.IncludeResolvedNameIfSuccessful, DirectlyIncludedFileNameFullPath);
					}
				}
				Includes[DirectlyIncludedFileNameIndex].HasAttemptedResolve = true;
				Includes[DirectlyIncludedFileNameIndex].IncludeResolvedNameIfSuccessful = DirectlyIncludedFileNameFullPath;
				if (DirectlyIncludedFileNameFullPath != null)
				{
					bIsDirty = true;
				}
			}
		}

		/// <summary>
		/// Gets the dependency cache path and filename for the specified target.
		/// </summary>
		/// <param name="Target">Current build target</param>
		/// <returns>Cache Path</returns>
		public static FileReference GetDependencyCachePathForTarget(UEBuildTarget Target)
		{
			DirectoryReference PlatformIntermediatePath;
			if (Target.ProjectFile != null)
			{
				PlatformIntermediatePath = DirectoryReference.Combine(Target.ProjectFile.Directory, BuildConfiguration.PlatformIntermediateFolder);
			}
			else
			{
				PlatformIntermediatePath = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, BuildConfiguration.PlatformIntermediateFolder);
			}
			return FileReference.Combine(PlatformIntermediatePath, Target.GetTargetName(), "DependencyCache.bin");
		}
	}
}
